#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <string.h>
#include <fcntl.h>
#define HISTORY_SIZE 5 
#define MAX_LINE 80

int main()
{
    int history = 0;
    char* HF[HISTORY_SIZE];
    int historyCount = 0;

    char *args[MAX_LINE / 2 + 1]; // Mảng để lưu trữ lệnh và các tham số của lệnh
    int should_run = 1;           // Biến cờ để xác định chương trình có tiếp tục chạy hay không

    // Cấp phát bộ nhớ
    for (int i = 0; i < MAX_LINE / 2 + 1; i++) {
        args[i] = malloc(MAX_LINE);
    }

    for (int i = 0; i < HISTORY_SIZE; i++) {
        HF[i] = malloc(MAX_LINE);
    }

    while (should_run)
    {
        printf("it007sh> "); // Dấu nhắc
        fflush(stdout); // Xóa bộ đệm
        
        char command[MAX_LINE]; // Khai báo câu lệnh có tối đa 80 ký tự
        fgets(command, MAX_LINE, stdin); // Nhập câu lệnh
        
        // Loại bỏ ký tự xuống dòng ('\n') ở cuối chuỗi nhập vào
        command[strcspn(command, "\n")] = '\0'; // strcspn tìm vị trí đầu tiên có '\n' trong chuỗi; '\0' kết thúc chuỗi tại đó
        
        // Lưu lịch sử lại câu lệnh vừa nhập
        if (historyCount < HISTORY_SIZE) {
            strcpy(HF[historyCount], command);
            historyCount++;
        } 

        // Nếu lịch sử đã đầy
        else {
            // Xóa lệnh đầu tiên và dịch chuyển các lệnh cũ lên trên
            for (int i = 0; i < HISTORY_SIZE - 1; i++) {
                strcpy(HF[i], HF[i + 1]);
            }
            strcpy(HF[HISTORY_SIZE - 1], command); // Lưu lệnh mới nhất vào cuối
        }

        // Xử lý chuỗi nhập vào
        int index = 0;
        char *token = strtok(command, " "); // Tách chuỗi ký tự đầu tiên ra khi có dấu cách và lưu vào token
        while (token != NULL) { 
            args[index] = token; // args[i] bằng một chuỗi ký tự ngăn bởi dấu cách hay nói cách khác là một từ; có tối đa 41 từ trên một dòng
            token = strtok(NULL, " "); // Tiếp tục tách chuỗi đến khi token là ký tự rỗng khi bị cắt bởi dấu cách
            index++;
        }
        args[index] = NULL; // Từ cuối cùng sẽ là kết thúc chuỗi từ 

        // Kiểm tra lệnh đặc biệt của người dùng nhập vào như "exit" 
        if (strcmp(args[0], "exit") == 0) {
            should_run = 0; // Dừng chương trình
            continue;
        }        

        // Nếu là pipeline
        int pipe_use = 0;
        int pipe_index;
        for (int temp = 0; temp < index; temp++) {
            // Dấu hiệu pipeline
            if (strcmp(args[temp], "|") == 0) {
                args[temp] = NULL; // Đánh dấu vị trí pipeline là NULL
                pipe_use = 1; // Cờ pipeline
                pipe_index = temp;
                break;
            }
        }
        if (pipe_use == 1) {
            int pipefd[2], status, done = 0;
            if (pipe(pipefd) == -1) {
                // Xử lý lỗi khi không thể tạo đường ống
                perror("Pipe fail. Try again");
            }
            pid_t cpid; 
            cpid = fork();
            if (cpid == 0) {
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                if (execvp(args[0], args) == -1 && strcmp(args[0],"HF") != 0) {
                    printf("Command does not exist.\n");
                    exit(EXIT_FAILURE);
                }
                exit(0);
            }
            cpid = fork();
            if (cpid == 0) {
                close(pipefd[1]);
                dup2(pipefd[0],STDIN_FILENO);
                if (execvp(args[pipe_index + 1], &args[pipe_index + 1]) == -1 && strcmp(args[0],"HF") != 0) {
                    printf("Command does not exist.\n");
                    exit(EXIT_FAILURE);
                }
                exit(0);
            }
            close (pipefd[0]);
            close (pipefd[1]);
            waitpid(-1, &status, 0);
            waitpid(-1, &status, 0);
            continue;
        }


        // Tiến trình con
        pid_t pid = fork();
        if (pid == 0) {
            // Nếu lệnh là "HF"
            if (strcmp(command, "HF") == 0) {
                // In ra 5 lệnh gần nhất
                for (int tempHF = 0; tempHF < historyCount; tempHF++) {
                    printf("%s\n", HF[tempHF]);
                }
                exit(0);
            }

            // Nếu lệnh là chuyển hướng đầu vào ra
            int input_fd, output_fd;
            int redirect_input = 0, redirect_output = 0;

            for (int j = 0; j < index; j++) {
                // Dấu hiệu chuyển hướng đầu ra của một tệp
                if (strcmp(args[j], ">") == 0) {
                    args[j] = NULL; // Đánh dấu vị trí chuyển hướng là NULL
                    redirect_output = 1; // Cờ chuyển hướng đầu ra

                    if (args[j + 1] == NULL) { // Nơi đầu ra được chuyển hướng vào là NULL nghĩa là không có file output.
                        printf("Missing output file\n");
                        exit(EXIT_FAILURE); // Báo lỗi xong thoát
                    }
                    // Tạo file args[i] nếu chưa có; file này sẽ thế chỗ đầu ra cho luồng chuẩn STDOUT 
                    output_fd = open(args[j + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644); // output_fd lưu trữ giá trị file descriptor nên phía trên mới khai báo int
                    if (output_fd < 0) {
                        perror("System fail to execuse.");
                        exit(EXIT_FAILURE); // Không mở được file nên báo lỗi rồi thoát
                    }
                    dup2(output_fd, STDOUT_FILENO); // Overwrite file STDOUT hiện tại bằng file output_fd vừa tạo, luồng đầu ra bây giờ là file output_fd
                    close(output_fd);
                
                // Dấu hiệu chuyển hướng đầu vào của một tệp
                } else if (strcmp(args[j], "<") == 0) {
                    args[j] = NULL; // Đánh dấu vị trí chuyển hướng là NULL
                    redirect_input = 1; // Cờ chuyển hướng đầu vào

                    if (args[j + 1] == NULL) { // Nơi đầu vào được chuyển hướng vào là NULL nghĩa là không có file input.
                        printf("Missing input file.\n");
                        exit(EXIT_FAILURE); // Báo lỗi xong thoát
                    }

                    // Tạo file args[i] nếu chưa có; file này sẽ thế chỗ đầu vào cho file STDIN của terminal
                    input_fd = open(args[j + 1], O_RDONLY); // input_fd lưu trữ giá trị file descriptor nên phía trên mới khai báo int
                    if (input_fd < 0) {
                        perror("System fail to execuse.");
                        exit(EXIT_FAILURE); // Không mở được file nên báo lỗi rồi thoát
                    }
                    dup2(input_fd, STDIN_FILENO); // Overwrite file STDIN hiện tại bằng file input_fd vừa tạo, luồng đầu vào bây giờ là file input_fd
                    close(input_fd);
                }
            }

            // Kiểm tra lỗi
            if (execvp(args[0], args) == -1 && strcmp(args[0],"HF") != 0) {
                printf("Command does not exist.\n");
                exit(EXIT_FAILURE);
            }
        }


        // Tiến trình cha
        else { 
            waitpid(pid, NULL, 0); // Chờ tiến trình con thực thi xong
        }
    }
    return 0;
}
