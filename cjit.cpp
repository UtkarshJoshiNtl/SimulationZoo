#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <string>
#include <vector>
#include <algorithm>

struct Commit {
    int id;
    std::string message;
    std::vector<std::string> files;
};

// Flat-file format: "id|message|file1,file2,...\n"
// Each file is stored as .cjit/objects/<id>_<filename>

static std::string read_file(const std::string& path) {
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) return "";
    std::string r;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
        r.append(buf, n);
    fclose(fp);
    return r;
}

static bool files_match(const std::string& a, const std::string& b) {
    FILE* fa = fopen(a.c_str(), "rb");
    FILE* fb = fopen(b.c_str(), "rb");
    if (!fa || !fb) {
        if (fa) fclose(fa);
        if (fb) fclose(fb);
        return false;
    }
    int ca, cb;
    bool same = true;
    while ((ca = fgetc(fa)) != EOF && (cb = fgetc(fb)) != EOF) {
        if (ca != cb) { same = false; break; }
    }
    if (same && (fgetc(fa) != EOF || fgetc(fb) != EOF)) same = false;
    fclose(fa); fclose(fb);
    return same;
}

static std::string obj_path(int id, const std::string& file) {
    char buf[512];
    snprintf(buf, sizeof(buf), ".cjit/objects/%d_%s", id, file.c_str());
    return buf;
}

static std::string read_head() {
    FILE* fp = fopen(".cjit/HEAD", "r");
    if (!fp) return "";
    char buf[256];
    if (!fgets(buf, sizeof(buf), fp)) { fclose(fp); return ""; }
    fclose(fp);
    buf[strcspn(buf, "\n")] = '\0';
    return buf;
}

static void write_head(const std::string& s) {
    FILE* fp = fopen(".cjit/HEAD", "w");
    if (fp) { fprintf(fp, "%s", s.c_str()); fclose(fp); }
}

static int read_branch_commit(const std::string& name) {
    FILE* fp = fopen(".cjit/branches.txt", "r");
    if (!fp) return -1;
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        char bname[256];
        int id;
        if (sscanf(line, "%255[^|]|%d", bname, &id) == 2 && name == bname) {
            fclose(fp);
            return id;
        }
    }
    fclose(fp);
    return -1;
}

static void update_branch_commit(const std::string& branch, int commit_id) {
    FILE* fp = fopen(".cjit/branches.txt", "r");
    if (!fp) return;
    FILE* tmp = fopen(".cjit/branches.tmp", "w");
    if (!tmp) { fclose(fp); return; }
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        char bname[256];
        int id;
        if (sscanf(line, "%255[^|]|%d", bname, &id) == 2) {
            if (bname == branch)
                fprintf(tmp, "%s|%d\n", branch.c_str(), commit_id);
            else
                fputs(line, tmp);
        }
    }
    fclose(fp);
    fclose(tmp);
    remove(".cjit/branches.txt");
    rename(".cjit/branches.tmp", ".cjit/branches.txt");
}

static int get_last_commit_id() {
    FILE* fp = fopen(".cjit/commits.txt", "r");
    if (!fp) return 0;
    char line[512];
    int last = 0;
    while (fgets(line, sizeof(line), fp)) {
        int id;
        if (sscanf(line, "%d|", &id) == 1 && id > last) last = id;
    }
    fclose(fp);
    return last;
}

// Parse a commit from commits.txt by its numeric ID.
// Format: "id|message|file1,file2,..."
// Temporarily replaces the second '|' with '\0' to isolate the message field,
// then restores it before tokenizing the file list.
static Commit parse_commit(int id) {
    Commit c;
    c.id = id;
    FILE* fp = fopen(".cjit/commits.txt", "r");
    if (!fp) return c;
    char line[2048];
    while (fgets(line, sizeof(line), fp)) {
        int lid;
        if (sscanf(line, "%d|", &lid) != 1 || lid != id) continue;
        char* p = strchr(line, '|');
        if (!p) continue;
        p = strchr(p + 1, '|');
        if (!p) continue;
        *p = '\0';
        char* msg = strchr(line, '|') + 1;
        c.message = msg;
        *p = '|';
        p++;
        char* tok = strtok(p, ",\n");
        while (tok) {
            c.files.push_back(tok);
            tok = strtok(NULL, ",\n");
        }
        break;
    }
    fclose(fp);
    return c;
}

static bool restore_files(const Commit& c) {
    bool ok = true;
    for (const auto& f : c.files) {
        std::string path = obj_path(c.id, f);
        FILE* src = fopen(path.c_str(), "r");
        if (!src) { ok = false; continue; }
        FILE* dst = fopen(f.c_str(), "w");
        if (!dst) { fclose(src); ok = false; continue; }
        int ch;
        while ((ch = fgetc(src)) != EOF) fputc(ch, dst);
        fclose(src); fclose(dst);
        printf("Restored: %s\n", f.c_str());
    }
    return ok;
}

static std::vector<std::string> read_staging() {
    std::vector<std::string> files;
    FILE* fp = fopen(".cjit/staging.txt", "r");
    if (!fp) return files;
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) > 0) files.push_back(line);
    }
    fclose(fp);
    return files;
}

static void write_staging(const std::vector<std::string>& files) {
    FILE* fp = fopen(".cjit/staging.txt", "w");
    if (!fp) return;
    for (const auto& f : files) fprintf(fp, "%s\n", f.c_str());
    fclose(fp);
}

// Line-by-line comparison. Unlike Myers diff, this only detects differing
// lines at the same position — insertions/deletions shift subsequent lines.
static int diff_files(const std::string& path_a, const std::string& path_b,
                      const std::string& label_a, const std::string& label_b,
                      const std::string& display) {
    FILE* fa = fopen(path_a.c_str(), "r");
    FILE* fb = fopen(path_b.c_str(), "r");
    if (!fa && !fb) return 0;
    int diffs = 0;
    char la[4096], lb[4096];
    int line = 1;
    while (1) {
        char* ra = fa ? fgets(la, sizeof(la), fa) : NULL;
        char* rb = fb ? fgets(lb, sizeof(lb), fb) : NULL;
        if (!ra && !rb) break;
        if (!ra) la[0] = '\0';
        if (!rb) lb[0] = '\0';
        if (strcmp(la, lb) != 0) {
            if (diffs == 0) printf("\n--- %s ---\n", display.c_str());
            printf("Line %d:\n", line);
            printf("  [%s] %s", label_a.c_str(), ra ? la : "");
            printf("  [%s] %s", label_b.c_str(), rb ? lb : "");
            diffs++;
        }
        line++;
    }
    if (fa) fclose(fa);
    if (fb) fclose(fb);
    return diffs;
}

// --- Commands ---

static int cmd_init() {
    struct stat st;
    if (stat(".cjit", &st) == 0) {
        printf("Already initialized\n");
        return 1;
    }
    mkdir(".cjit", 0755);
    mkdir(".cjit/objects", 0755);
    FILE* f = fopen(".cjit/commits.txt", "w");
    if (!f) { perror("Failed to create commits.txt"); return 1; }
    fclose(f);
    f = fopen(".cjit/staging.txt", "w");
    if (!f) { perror("Failed to create staging.txt"); return 1; }
    fclose(f);
    f = fopen(".cjit/branches.txt", "w");
    if (!f) { perror("Failed to create branches.txt"); return 1; }
    fprintf(f, "main|0\n"); fclose(f);
    write_head("main");
    printf("Initialized empty CJIT repository\n");
    return 0;
}

static int cmd_add(const std::string& file) {
    FILE* fp = fopen(file.c_str(), "r");
    if (!fp) { printf("Error: cannot open %s\n", file.c_str()); return 1; }
    fclose(fp);
    auto staged = read_staging();
    if (std::find(staged.begin(), staged.end(), file) != staged.end()) {
        printf("Already staged: %s\n", file.c_str());
        return 1;
    }
    fp = fopen(".cjit/staging.txt", "a");
    if (!fp) return 1;
    fprintf(fp, "%s\n", file.c_str());
    fclose(fp);
    printf("Staged: %s\n", file.c_str());
    return 0;
}

static int cmd_rm(const std::string& file) {
    auto files = read_staging();
    auto it = std::find(files.begin(), files.end(), file);
    if (it == files.end()) {
        printf("Not staged: %s\n", file.c_str());
        return 1;
    }
    files.erase(it);
    write_staging(files);
    printf("Unstaged: %s\n", file.c_str());
    return 0;
}

static int cmd_commit(const std::string& msg) {
    auto staged = read_staging();
    if (staged.empty()) {
        printf("Nothing to commit\n");
        return 1;
    }
    for (const auto& f : staged) {
        FILE* check = fopen(f.c_str(), "r");
        if (!check) {
            printf("Error: cannot read '%s' (deleted after staging)\n", f.c_str());
            return 1;
        }
        fclose(check);
    }
    int id = get_last_commit_id() + 1;
    FILE* fp = fopen(".cjit/commits.txt", "a");
    if (!fp) return 1;
    fprintf(fp, "%d|%s|", id, msg.c_str());
    for (size_t i = 0; i < staged.size(); i++) {
        if (i > 0) fprintf(fp, ",");
        fprintf(fp, "%s", staged[i].c_str());
    }
    fprintf(fp, "\n");
    fclose(fp);
    for (const auto& f : staged) {
        std::string path = obj_path(id, f);
        FILE* src = fopen(f.c_str(), "r");
        if (!src) { printf("Warning: cannot read %s\n", f.c_str()); continue; }
        FILE* dst = fopen(path.c_str(), "w");
        if (!dst) { fclose(src); continue; }
        int ch;
        while ((ch = fgetc(src)) != EOF) fputc(ch, dst);
        fclose(src); fclose(dst);
    }
    std::string head = read_head();
    int bid = read_branch_commit(head);
    if (bid >= 0)
        update_branch_commit(head, id);
    else
        write_head(std::to_string(id));
    FILE* sf = fopen(".cjit/staging.txt", "w");
    if (sf) fclose(sf);
    printf("Committed: %s (id: %d, files: %zu)\n", msg.c_str(), id, staged.size());
    return 0;
}

static int cmd_log(bool oneline) {
    FILE* fp = fopen(".cjit/commits.txt", "r");
    if (!fp) { printf("No commits\n"); return 1; }
    printf("Commit history:\n");
    char line[2048];
    while (fgets(line, sizeof(line), fp)) {
        char* p = strchr(line, '|');
        if (!p) continue;
        p = strchr(p + 1, '|');
        if (!p) continue;
        *p = '\0';
        int id;
        sscanf(line, "%d", &id);
        char* msg = strchr(line, '|') + 1;
        if (oneline)
            printf("  %d: %s\n", id, msg);
        else
            printf("  Commit %d: %s\n", id, msg);
    }
    fclose(fp);
    return 0;
}

static int cmd_diff_working() {
    int last = get_last_commit_id();
    if (last == 0) { printf("No commits\n"); return 1; }
    Commit c = parse_commit(last);
    printf("Diff between commit %d and working tree:\n", last);
    int total = 0;
    for (const auto& f : c.files) {
        std::string op = obj_path(last, f);
        total += diff_files(op, f, std::to_string(last), "wt", f);
    }
    auto staged = read_staging();
    for (const auto& f : staged) {
        if (std::find(c.files.begin(), c.files.end(), f) == c.files.end()) {
            printf("\n--- %s ---\n", f.c_str());
            printf("  New file in working tree: %s (not yet committed)\n", f.c_str());
            total++;
        }
    }
    if (!total) printf("  No differences\n");
    return 0;
}

static int cmd_diff(int id1, int id2) {
    Commit c1 = parse_commit(id1);
    Commit c2 = parse_commit(id2);
    if (c1.files.empty() && c2.files.empty()) {
        printf("No files to compare\n");
        return 1;
    }
    printf("Diff between commit %d and commit %d:\n", id1, id2);
    int total = 0;
    for (const auto& f1 : c1.files) {
        auto it = std::find(c2.files.begin(), c2.files.end(), f1);
        if (it != c2.files.end()) {
            std::string p1 = obj_path(id1, f1);
            std::string p2 = obj_path(id2, *it);
            total += diff_files(p1, p2, std::to_string(id1), std::to_string(id2), f1);
        } else {
            printf("  Only in commit %d: %s\n", id1, f1.c_str());
            total++;
        }
    }
    for (const auto& f2 : c2.files) {
        if (std::find(c1.files.begin(), c1.files.end(), f2) == c1.files.end()) {
            printf("  Only in commit %d: %s\n", id2, f2.c_str());
            total++;
        }
    }
    if (!total) printf("  No differences\n");
    return 0;
}

static int cmd_branch_list() {
    std::string cur = read_head();
    FILE* fp = fopen(".cjit/branches.txt", "r");
    if (!fp) return 0;
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        char name[256];
        int id;
        if (sscanf(line, "%255[^|]|%d", name, &id) == 2) {
            if (name == cur)
                printf("* %s (%d)\n", name, id);
            else
                printf("  %s (%d)\n", name, id);
        }
    }
    fclose(fp);
    return 0;
}

static int cmd_branch_create(const std::string& name) {
    std::string head = read_head();
    int bid = read_branch_commit(head);
    if (bid < 0) {
        char* end;
        long id = strtol(head.c_str(), &end, 10);
        if (*end != '\0' || id <= 0) {
            printf("Unknown HEAD state\n");
            return 1;
        }
        bid = (int)id;
    }
    if (read_branch_commit(name) >= 0) {
        printf("Branch already exists: %s\n", name.c_str());
        return 1;
    }
    FILE* fp = fopen(".cjit/branches.txt", "a");
    if (!fp) return 1;
    fprintf(fp, "%s|%d\n", name.c_str(), bid);
    fclose(fp);
    printf("Created branch: %s\n", name.c_str());
    return 0;
}

static int cmd_branch_delete(const std::string& name) {
    std::string cur = read_head();
    if (cur == name) {
        printf("Cannot delete current branch\n");
        return 1;
    }
    FILE* fp = fopen(".cjit/branches.txt", "r");
    if (!fp) return 1;
    std::vector<std::pair<std::string, int>> branches;
    char line[512];
    bool found = false;
    while (fgets(line, sizeof(line), fp)) {
        char bname[256];
        int bid;
        if (sscanf(line, "%255[^|]|%d", bname, &bid) == 2) {
            if (bname == name) { found = true; continue; }
            branches.push_back({bname, bid});
        }
    }
    fclose(fp);
    if (!found) { printf("Branch not found: %s\n", name.c_str()); return 1; }
    fp = fopen(".cjit/branches.txt", "w");
    for (auto& b : branches)
        fprintf(fp, "%s|%d\n", b.first.c_str(), b.second);
    fclose(fp);
    printf("Deleted branch: %s\n", name.c_str());
    return 0;
}

static int cmd_checkout(const std::string& target) {
    int bid = read_branch_commit(target);
    if (bid >= 0) {
        write_head(target);
        printf("Switched to branch: %s\n", target.c_str());
        if (bid > 0) {
            Commit c = parse_commit(bid);
            if (!c.files.empty()) {
                printf("(at commit %d)\n", bid);
                restore_files(c);
            }
        }
        return 0;
    }
    char* end;
    long id = strtol(target.c_str(), &end, 10);
    if (*end != '\0' || id <= 0) {
        printf("Not found: %s\n", target.c_str());
        return 1;
    }
    Commit c = parse_commit((int)id);
    if (c.files.empty()) {
        printf("Commit %ld not found\n", id);
        return 1;
    }
    write_head(std::to_string(id));
    restore_files(c);
    printf("Checked out commit: %ld\n", id);
    return 0;
}

static int cmd_status() {
    std::string head = read_head();
    if (head.empty()) { printf("Not a repository\n"); return 1; }
    if (read_branch_commit(head) < 0) {
        char* end;
        long head_id = strtol(head.c_str(), &end, 10);
        if (*end == '\0' && head_id > 0)
            printf("(detached HEAD at commit %ld)\n", head_id);
        else
            printf("(detached HEAD: %s)\n", head.c_str());
    } else {
        printf("On branch: %s\n", head.c_str());
    }
    auto staged = read_staging();
    printf("Staged files:\n");
    if (staged.empty())
        printf("  (none)\n");
    else
        for (auto& f : staged) printf("  %s\n", f.c_str());
    int last = get_last_commit_id();
    if (last > 0) {
        Commit c = parse_commit(last);
        bool unstaged = false;
        for (const auto& f : c.files) {
            std::string op = obj_path(last, f);
            FILE* tfp = fopen(f.c_str(), "r");
            if (!tfp) {
                if (!unstaged) printf("Unstaged changes:\n");
                printf("  deleted: %s\n", f.c_str());
                unstaged = true;
            } else {
                fclose(tfp);
                if (!files_match(op, f)) {
                    if (!unstaged) printf("Unstaged changes:\n");
                    printf("  modified: %s\n", f.c_str());
                    unstaged = true;
                }
            }
        }
        if (!unstaged) printf("No unstaged changes\n");
        printf("Last commit: %d - %s\n", c.id, c.message.c_str());
    } else {
        printf("No commits yet\n");
    }
    return 0;
}

// --- Merge / Rebase ---

static int cmd_merge(const std::string& branch) {
    std::string cur_name = read_head();
    if (cur_name.empty()) { printf("Not on a branch\n"); return 1; }
    int cur_id = read_branch_commit(cur_name);
    if (cur_id < 0) { printf("No commits on current branch\n"); return 1; }
    if (branch == cur_name) { printf("Cannot merge branch into itself\n"); return 1; }
    int other_id = read_branch_commit(branch);
    if (other_id < 0) { printf("Branch not found: %s\n", branch.c_str()); return 1; }
    if (other_id == 0) { printf("Nothing to merge\n"); return 1; }
    Commit cur = parse_commit(cur_id);
    Commit other = parse_commit(other_id);
    std::vector<std::string> merged = cur.files;
    bool conflict = false;
    for (const auto& f : other.files) {
        bool in_cur = std::find(cur.files.begin(), cur.files.end(), f) != cur.files.end();
        if (!in_cur) {
            std::string src = obj_path(other_id, f);
            FILE* s = fopen(src.c_str(), "r");
            FILE* d = fopen(f.c_str(), "w");
            if (s && d) { int ch; while ((ch = fgetc(s)) != EOF) fputc(ch, d); }
            if (s) fclose(s);
            if (d) fclose(d);
            merged.push_back(f);
            printf("Added: %s\n", f.c_str());
        } else {
            std::string cur_path = obj_path(cur_id, f);
            std::string other_path = obj_path(other_id, f);
            if (!files_match(cur_path, other_path)) {
                std::string cur_c = read_file(cur_path);
                std::string other_c = read_file(other_path);
                FILE* d = fopen(f.c_str(), "w");
                if (d) {
                    fprintf(d, "<<<<<<< %s\n", cur_name.c_str());
                    fputs(cur_c.c_str(), d);
                    fprintf(d, "=======\n");
                    fputs(other_c.c_str(), d);
                    fprintf(d, ">>>>>>> %s\n", branch.c_str());
                    fclose(d);
                }
                conflict = true;
                printf("Conflict: %s\n", f.c_str());
            }
        }
    }
    std::string msg = "Merge branch '" + branch + "' into " + cur_name;
    if (conflict) {
        printf("\nConflicts remain - resolve them and run 'cjit commit \"%s\"' to complete the merge\n", msg.c_str());
        return 0;
    }
    write_staging(merged);
    cmd_commit(msg);
    return 0;
}

static int cmd_rebase(const std::string& branch) {
    std::string cur_name = read_head();
    if (cur_name.empty()) { printf("Not on a branch\n"); return 1; }
    int cur_id = read_branch_commit(cur_name);
    if (cur_id < 0) { printf("No commits on current branch\n"); return 1; }
    if (branch == cur_name) { printf("Cannot rebase onto itself\n"); return 1; }
    int base_id = read_branch_commit(branch);
    if (base_id < 0) { printf("Branch not found: %s\n", branch.c_str()); return 1; }
    if (base_id == 0) { printf("Target branch has no commits\n"); return 1; }
    if (cur_id == base_id) { printf("Already up to date\n"); return 0; }
    Commit base = parse_commit(base_id);
    Commit cur = parse_commit(cur_id);
    restore_files(base);
    for (const auto& f : cur.files) {
        bool in_base = std::find(base.files.begin(), base.files.end(), f) != base.files.end();
        if (!in_base) {
            std::string src = obj_path(cur_id, f);
            FILE* s = fopen(src.c_str(), "r");
            FILE* d = fopen(f.c_str(), "w");
            if (s && d) { int ch; while ((ch = fgetc(s)) != EOF) fputc(ch, d); }
            if (s) fclose(s);
            if (d) fclose(d);
        } else {
            std::string cur_path = obj_path(cur_id, f);
            std::string base_path = obj_path(base_id, f);
            if (!files_match(cur_path, base_path)) {
                FILE* s = fopen(cur_path.c_str(), "r");
                FILE* d = fopen(f.c_str(), "w");
                if (s && d) { int ch; while ((ch = fgetc(s)) != EOF) fputc(ch, d); }
                if (s) fclose(s);
                if (d) fclose(d);
            }
        }
    }
    std::vector<std::string> rebased;
    for (const auto& f : base.files) {
        if (std::find(cur.files.begin(), cur.files.end(), f) == cur.files.end())
            rebased.push_back(f);
    }
    for (const auto& f : cur.files) rebased.push_back(f);
    write_staging(rebased);
    std::string msg = "Rebased onto " + branch;
    cmd_commit(msg);
    printf("Rebased %s onto %s\n", cur_name.c_str(), branch.c_str());
    return 0;
}

// --- Main ---

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: cjit <command>\n"
               "Commands: init, add, rm, commit, log, diff, branch, checkout, status, merge, rebase\n");
        return 1;
    }
    std::string cmd = argv[1];
    if (cmd == "init") return cmd_init();
    if (cmd == "add") {
        if (argc < 3) { printf("Usage: cjit add <file>\n"); return 1; }
        return cmd_add(argv[2]);
    }
    if (cmd == "rm") {
        if (argc < 3) { printf("Usage: cjit rm <file>\n"); return 1; }
        return cmd_rm(argv[2]);
    }
    if (cmd == "commit") {
        if (argc < 3) { printf("Usage: cjit commit <message>\n"); return 1; }
        return cmd_commit(argv[2]);
    }
    if (cmd == "log") {
        bool oneline = argc > 2 && strcmp(argv[2], "--oneline") == 0;
        return cmd_log(oneline);
    }
    if (cmd == "diff") {
        if (argc == 2) return cmd_diff_working();
        if (argc == 4) return cmd_diff(atoi(argv[2]), atoi(argv[3]));
        printf("Usage: cjit diff [<id1> <id2>]\n");
        return 1;
    }
    if (cmd == "branch") {
        if (argc < 3) return cmd_branch_list();
        if (strcmp(argv[2], "-d") == 0) {
            if (argc < 4) { printf("Usage: cjit branch -d <name>\n"); return 1; }
            return cmd_branch_delete(argv[3]);
        }
        return cmd_branch_create(argv[2]);
    }
    if (cmd == "checkout") {
        if (argc < 3) { printf("Usage: cjit checkout <branch_or_id>\n"); return 1; }
        return cmd_checkout(argv[2]);
    }
    if (cmd == "status") return cmd_status();
    if (cmd == "merge") {
        if (argc < 3) { printf("Usage: cjit merge <branch>\n"); return 1; }
        return cmd_merge(argv[2]);
    }
    if (cmd == "rebase") {
        if (argc < 3) { printf("Usage: cjit rebase <branch>\n"); return 1; }
        return cmd_rebase(argv[2]);
    }
    fprintf(stderr, "Unknown command: %s\n", cmd.c_str());
    return 1;
}
