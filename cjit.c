#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

struct Commit {
    int id;
    char message[100];
    char files[10][100];
    int file_count;
};

void init_repo() {
    mkdir(".cjit", 0755);
    mkdir(".cjit/objects", 0755);
    FILE *f1 = fopen(".cjit/commits.txt", "w");
    FILE *f2 = fopen(".cjit/staging.txt", "w");
    FILE *f3 = fopen(".cjit/branches.txt", "w");
    fprintf(f3, "main|0\n");
    FILE *f4 = fopen(".cjit/HEAD", "w");
    fprintf(f4, "main");
    fclose(f1);
    fclose(f2);
    fclose(f3);
    fclose(f4);
    printf("Initialized empty CJIT repository\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: cjit <command>\n");
        return 1;
    }

    if (strcmp(argv[1], "init") == 0) {
        init_repo();
    } else if (strcmp(argv[1], "add") == 0) {
        if (argc < 3) {
            printf("Usage: cjit add <filename>\n");
            return 1;
        }
        FILE *f = fopen(argv[2], "r");
        if (!f) {
            printf("Error: cannot open %s\n", argv[2]);
            return 1;
        }
        fclose(f);
        FILE *staging = fopen(".cjit/staging.txt", "a");
        fprintf(staging, "%s\n", argv[2]);
        fclose(staging);
        printf("Staged: %s\n", argv[2]);
    } else if (strcmp(argv[1], "commit") == 0) {
        if (argc < 3) {
            printf("Usage: cjit commit <message>\n");
            return 1;
        }
        // Read last commit ID from commits file to avoid reset on program restart
        int commit_id = 1;
        FILE *existing_commits = fopen(".cjit/commits.txt", "r");
        if (existing_commits) {
            char last_line[512];
            while (fgets(last_line, 512, existing_commits)) {
                int id;
                if (sscanf(last_line, "%d|", &id) == 1) {
                    commit_id = id + 1;
                }
            }
            fclose(existing_commits);
        }
        
        struct Commit c;
        c.id = commit_id;
        strcpy(c.message, argv[2]);
        c.file_count = 0;

        FILE *commits = fopen(".cjit/commits.txt", "a");
        fprintf(commits, "%d|%s|", c.id, c.message);

        char staged_file[128];
        FILE *staging = fopen(".cjit/staging.txt", "r");
        while (fgets(staged_file, 128, staging) && c.file_count < 10) {
            staged_file[strcspn(staged_file, "\n")] = '\0';
            strcpy(c.files[c.file_count], staged_file);
            c.file_count++;
            fprintf(commits, "%s,", staged_file);

            char obj_path[256];
            sprintf(obj_path, ".cjit/objects/%d_%s", c.id, staged_file);
            FILE *src = fopen(staged_file, "r");
            FILE *dest = fopen(obj_path, "w");
            char ch;
            while ((ch = fgetc(src)) != EOF) {
                fputc(ch, dest);
            }
            fclose(src);
            fclose(dest);
        }
        fprintf(commits, "\n");
        fclose(staging);
        fclose(commits);

        FILE *clear = fopen(".cjit/staging.txt", "w");
        fclose(clear);
        printf("Committed: %s (id: %d, files: %d)\n", c.message, c.id, c.file_count);

        // Update current branch pointer to this commit
        char head_branch[100];
        FILE *hfp = fopen(".cjit/HEAD", "r");
        if (hfp) {
            fgets(head_branch, 100, hfp);
            head_branch[strcspn(head_branch, "\n")] = '\0';
            fclose(hfp);
            FILE *bfp = fopen(".cjit/branches.txt", "r");
            FILE *tmp = fopen(".cjit/branches.tmp", "w");
            char bline[256];
            while (fgets(bline, 256, bfp)) {
                char bname[100];
                int bid;
                if (sscanf(bline, "%[^|]|%d", bname, &bid) == 2) {
                    if (strcmp(bname, head_branch) == 0) {
                        fprintf(tmp, "%s|%d\n", bname, c.id);
                    } else {
                        fputs(bline, tmp);
                    }
                }
            }
            fclose(bfp);
            fclose(tmp);
            remove(".cjit/branches.txt");
            rename(".cjit/branches.tmp", ".cjit/branches.txt");
        }
    } else if (strcmp(argv[1], "log") == 0) {
        FILE *commits = fopen(".cjit/commits.txt", "r");
        if (!commits) {
            printf("No commits found\n");
            return 1;
        }
        char line[512];
        printf("Commit history:\n");
        while (fgets(line, 512, commits)) {
            int id;
            char msg[100];
            // Parse the message properly - it's between the second pipe and the file list
            char *first_pipe = strchr(line, '|');
            if (first_pipe) {
                char *second_pipe = strchr(first_pipe + 1, '|');
                if (second_pipe) {
                    *second_pipe = '\0';
                    sscanf(line, "%d|%99[^\n]", &id, msg);
                    *second_pipe = '|'; // restore for potential reuse
                } else {
                    sscanf(line, "%d|%99[^\n]", &id, msg);
                }
            }
            printf("  Commit %d: %s\n", id, msg);
        }
        fclose(commits);
    } else if (strcmp(argv[1], "diff") == 0) {
        if (argc < 4) {
            printf("Usage: cjit diff <commit1> <commit2>\n");
            return 1;
        }
        int id1 = atoi(argv[2]);
        int id2 = atoi(argv[3]);
        printf("Diff between commit %d and commit %d:\n", id1, id2);

        // Read file lists from both commits
        char files1[10][100], files2[10][100];
        int count1 = 0, count2 = 0;
        
        FILE *commits = fopen(".cjit/commits.txt", "r");
        char line[512];
        while (fgets(line, 512, commits)) {
            int id;
            sscanf(line, "%d|", &id);
            if (id == id1) {
                char *files_start = strchr(line, '|');
                files_start = strchr(files_start + 1, '|');
                if (files_start) {
                    files_start++;
                    char *file = strtok(files_start, ",\n");
                    while (file && count1 < 10) {
                        strcpy(files1[count1++], file);
                        file = strtok(NULL, ",\n");
                    }
                }
            } else if (id == id2) {
                char *files_start = strchr(line, '|');
                files_start = strchr(files_start + 1, '|');
                if (files_start) {
                    files_start++;
                    char *file = strtok(files_start, ",\n");
                    while (file && count2 < 10) {
                        strcpy(files2[count2++], file);
                        file = strtok(NULL, ",\n");
                    }
                }
            }
        }
        fclose(commits);

        // Compare files that exist in both commits
        int total_diffs = 0;
        for (int i = 0; i < count1; i++) {
            for (int j = 0; j < count2; j++) {
                if (strcmp(files1[i], files2[j]) == 0) {
                    char file1_path[256], file2_path[256];
                    sprintf(file1_path, ".cjit/objects/%d_%s", id1, files1[i]);
                    sprintf(file2_path, ".cjit/objects/%d_%s", id2, files2[j]);

                    FILE *f1 = fopen(file1_path, "r");
                    FILE *f2 = fopen(file2_path, "r");

                    if (!f1 || !f2) {
                        printf("Cannot open object files for %s\n", files1[i]);
                        if (f1) fclose(f1);
                        if (f2) fclose(f2);
                        continue;
                    }

                    char line1[256], line2[256];
                    int line_num = 1;
                    int file_diff = 0;

                    printf("\n--- %s ---\n", files1[i]);
                    while (1) {
                        char *r1 = fgets(line1, 256, f1);
                        char *r2 = fgets(line2, 256, f2);
                        if (!r1 && !r2) break;
                        if (!r1) line1[0] = '\0';
                        if (!r2) line2[0] = '\0';
                        if (strcmp(line1, line2) != 0) {
                            printf("Line %d:\n", line_num);
                            printf("  [%d] %s", id1, line1);
                            printf("  [%d] %s", id2, line2);
                            file_diff = 1;
                            total_diffs = 1;
                        }
                        line_num++;
                    }

                    if (!file_diff) {
                        printf("  No differences in this file\n");
                    }

                    fclose(f1);
                    fclose(f2);
                }
            }
        }

        if (!total_diffs) {
            printf("  No differences found\n");
        }
    } else if (strcmp(argv[1], "branch") == 0) {
        if (argc < 3) {
            printf("Usage: cjit branch <name>\n");
            return 1;
        }
        int branch_start = 0;
        FILE *hfp = fopen(".cjit/HEAD", "r");
        if (hfp) {
            char cur_branch[100];
            fgets(cur_branch, 100, hfp);
            cur_branch[strcspn(cur_branch, "\n")] = '\0';
            fclose(hfp);
            FILE *bfp = fopen(".cjit/branches.txt", "r");
            if (bfp) {
                char bline[256];
                while (fgets(bline, 256, bfp)) {
                    char bname[100];
                    int bid;
                    if (sscanf(bline, "%[^|]|%d", bname, &bid) == 2) {
                        if (strcmp(bname, cur_branch) == 0) {
                            branch_start = bid;
                            break;
                        }
                    }
                }
                fclose(bfp);
            }
        }
        FILE *branches = fopen(".cjit/branches.txt", "a");
        fprintf(branches, "%s|%d\n", argv[2], branch_start);
        fclose(branches);
        printf("Created branch: %s\n", argv[2]);
    } else if (strcmp(argv[1], "status") == 0) {
        FILE *head = fopen(".cjit/HEAD", "r");
        if (!head) {
            printf("No repository found\n");
            return 1;
        }
        char current_branch[100];
        fgets(current_branch, 100, head);
        current_branch[strcspn(current_branch, "\n")] = '\0';
        fclose(head);
        
        printf("On branch: %s\n", current_branch);
        
        // Show staged files
        FILE *staging = fopen(".cjit/staging.txt", "r");
        if (staging) {
            char staged_file[128];
            int staged_count = 0;
            printf("Staged files:\n");
            while (fgets(staged_file, 128, staging)) {
                staged_file[strcspn(staged_file, "\n")] = '\0';
                if (strlen(staged_file) > 0) {
                    printf("  %s\n", staged_file);
                    staged_count++;
                }
            }
            fclose(staging);
            if (staged_count == 0) {
                printf("  (no staged files)\n");
            }
        }
        
        // Show last commit
        FILE *commits = fopen(".cjit/commits.txt", "r");
        if (commits) {
            char last_line[512];
            char *last_commit = NULL;
            while (fgets(last_line, 512, commits)) {
                last_commit = strdup(last_line);
            }
            if (last_commit) {
                int id;
                char msg[100];
                char *first_pipe = strchr(last_commit, '|');
                if (first_pipe) {
                    char *second_pipe = strchr(first_pipe + 1, '|');
                    if (second_pipe) {
                        *second_pipe = '\0';
                        sscanf(last_commit, "%d|%99[^\n]", &id, msg);
                        printf("Last commit: %d - %s\n", id, msg);
                    }
                }
                free(last_commit);
            }
            fclose(commits);
        }
    } else if (strcmp(argv[1], "checkout") == 0) {
        if (argc < 3) {
            printf("Usage: cjit checkout <commit_id_or_branch>\n");
            return 1;
        }
        int is_branch = 0;
        int target_commit = 0;
        char target_branch[100];

        FILE *branches = fopen(".cjit/branches.txt", "r");
        char line[256];
        while (fgets(line, 256, branches)) {
            char bname[100];
            int bid;
            sscanf(line, "%[^|]|%d", bname, &bid);
            if (strcmp(bname, argv[2]) == 0) {
                is_branch = 1;
                strcpy(target_branch, bname);
                target_commit = bid;
                break;
            }
        }
        fclose(branches);

        if (is_branch) {
            FILE *head = fopen(".cjit/HEAD", "w");
            fprintf(head, "%s", target_branch);
            fclose(head);
            printf("Switched to branch: %s\n", target_branch);
            if (target_commit > 0) {
                printf("(at commit %d)\n", target_commit);
                // Restore files for the branch's commit
                int target_id = target_commit;
                FILE *commits = fopen(".cjit/commits.txt", "r");
                char cline[512];
                int found = 0;
                while (fgets(cline, 512, commits)) {
                    int id;
                    sscanf(cline, "%d|", &id);
                    if (id == target_id) {
                        found = 1;
                        char *files_start = strchr(cline, '|');
                        files_start = strchr(files_start + 1, '|');
                        if (files_start) {
                            files_start++;
                            char *files = strtok(files_start, ",");
                            while (files && strcmp(files, "\n") != 0) {
                                char obj_path[256];
                                sprintf(obj_path, ".cjit/objects/%d_%s", target_id, files);
                                FILE *src = fopen(obj_path, "r");
                                FILE *dest = fopen(files, "w");
                                char ch;
                                while ((ch = fgetc(src)) != EOF) {
                                    fputc(ch, dest);
                                }
                                fclose(src);
                                fclose(dest);
                                printf("Restored: %s\n", files);
                                files = strtok(NULL, ",");
                            }
                        }
                        break;
                    }
                }
                fclose(commits);
                if (!found) {
                    printf("Commit %d not found\n", target_id);
                }
            }
        } else {
            int commit_id = atoi(argv[2]);
            FILE *commits = fopen(".cjit/commits.txt", "r");
            char cline[512];
            int found = 0;
            while (fgets(cline, 512, commits)) {
                int cid;
                sscanf(cline, "%d|", &cid);
                if (cid == commit_id) {
                    found = 1;
                    // Restore files for the commit
                    char *files_start = strchr(cline, '|');
                    files_start = strchr(files_start + 1, '|');
                    if (files_start) {
                        files_start++;
                        char *files = strtok(files_start, ",");
                        while (files && strcmp(files, "\n") != 0) {
                            char obj_path[256];
                            sprintf(obj_path, ".cjit/objects/%d_%s", commit_id, files);
                            FILE *src = fopen(obj_path, "r");
                            FILE *dest = fopen(files, "w");
                            char ch;
                            while ((ch = fgetc(src)) != EOF) {
                                fputc(ch, dest);
                            }
                            fclose(src);
                            fclose(dest);
                            printf("Restored: %s\n", files);
                            files = strtok(NULL, ",");
                        }
                    }
                    break;
                }
            }
            fclose(commits);
            if (found) {
                FILE *head = fopen(".cjit/HEAD", "w");
                fprintf(head, "%d", commit_id);
                fclose(head);
                printf("Checked out commit: %d\n", commit_id);
            } else {
                printf("Branch or commit not found: %s\n", argv[2]);
            }
        }
    } else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        return 1;
    }

    return 0;
}
