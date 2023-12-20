#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>

#define HISTORY_SIZE 5
#define MAX_LINE 80
#define MAX_COMMAND 4 // Số command tối đa được sử dụng trong pipe, ví dụ: 'ls | grep a | wc -l' là 3 command

char *HF[HISTORY_SIZE];       // Mảng để lưu lịch sử các câu lệnh
char *args[MAX_LINE / 2 + 1]; // Mảng để lưu trữ lệnh và các tham số của lệnh
int historyCount = 0;
int argsCount;
int childRunning = 0; // Biến cờ để xác định process con có đang chạy hay không
pid_t pid;

// Hàm để kill tiến trình con
void sigint_handler()
{
    if (pid != 0 && childRunning == 1)
        kill(pid, SIGKILL);
    else
        return;
}

void exec_cmd(char *command)
{
    argsCount = 0;

    // Xử lý chuỗi nhập vào
    char *token = strtok(command, " "); // Tách chuỗi ký tự đầu tiên ra khi có dấu cách và lưu vào token
    while (token != NULL)
    {
        args[argsCount++] = token; // args[i] bằng một chuỗi ký tự ngăn bởi dấu cách hay nói cách khác là một từ; có tối đa 41 từ trên một dòng
        token = strtok(NULL, " "); // Tiếp tục tách chuỗi đến khi token là ký tự rỗng khi bị cắt bởi dấu cách
    }
    args[argsCount] = NULL; // Từ cuối cùng sẽ là kết thúc chuỗi từ

    // Nếu lệnh là chuyển hướng đầu vào ra
    int input_fd, output_fd;
    int redirect_input = 0, redirect_output = 0;

    for (int j = 0; j < argsCount; j++)
    {
        // Dấu hiệu chuyển hướng đầu ra của một tệp
        if (strcmp(args[j], ">") == 0)
        {
            args[j] = NULL;      // Đánh dấu vị trí chuyển hướng là NULL
            redirect_output = 1; // Cờ chuyển hướng đầu ra

            if (args[j + 1] == NULL)
            { // Nơi đầu ra được chuyển hướng vào là NULL nghĩa là không có file output.
                printf("Missing output file\n");
                exit(EXIT_FAILURE); // Báo lỗi xong thoát
            }
            // Tạo file args[i] nếu chưa có; file này sẽ thế chỗ đầu ra cho luồng chuẩn STDOUT
            output_fd = open(args[j + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644); // output_fd lưu trữ giá trị file descriptor nên phía trên mới khai báo int
            if (output_fd < 0)
            {
                perror("System fail to execute.");
                exit(EXIT_FAILURE); // Không mở được file nên báo lỗi rồi thoát
            }
            dup2(output_fd, STDOUT_FILENO); // Overwrite file STDOUT hiện tại bằng file output_fd vừa tạo, luồng đầu ra bây giờ là file output_fd
            close(output_fd);
        }

        // Dấu hiệu chuyển hướng đầu vào của một tệp
        else if (strcmp(args[j], "<") == 0)
        {
            args[j] = NULL;     // Đánh dấu vị trí chuyển hướng là NULL
            redirect_input = 1; // Cờ chuyển hướng đầu vào

            if (args[j + 1] == NULL)
            { // Nơi đầu vào được chuyển hướng vào là NULL nghĩa là không có file input.
                printf("Missing input file.\n");
                exit(EXIT_FAILURE); // Báo lỗi xong thoát
            }

            // Tạo file args[i] nếu chưa có; file này sẽ thế chỗ đầu vào cho file STDIN của terminal
            input_fd = open(args[j + 1], O_RDONLY); // input_fd lưu trữ giá trị file descriptor nên phía trên mới khai báo int
            if (input_fd < 0)
            {
                perror("System fail to execuse.");
                exit(EXIT_FAILURE); // Không mở được file nên báo lỗi rồi thoát
            }
            dup2(input_fd, STDIN_FILENO); // Overwrite file STDIN hiện tại bằng file input_fd vừa tạo, luồng đầu vào bây giờ là file input_fd
            close(input_fd);
        }
    }

    // Kiểm tra lỗi
    if (execvp(args[0], args) == -1)
    {
        printf("Command does not exist.\n");
        exit(EXIT_FAILURE);
    }
}

int main()
{
    // Cấp phát bộ nhớ
    for (int i = 0; i < HISTORY_SIZE; i++)
    {
        HF[i] = malloc(MAX_LINE);
    }
    for (int i = 0; i < MAX_LINE / 2 + 1; i++)
    {
        args[i] = malloc(MAX_LINE);
    }

    signal(SIGINT, sigint_handler); // Listener để xử lý khi khi người dùng nhấn Ctrl + C

    int should_run = 1; // Biến cờ để xác định chương trình có tiếp tục chạy hay không
    while (should_run)
    {
        printf("it007sh> "); // Dấu nhắc
        fflush(stdout);      // Xóa bộ đệm

        char command[MAX_LINE];
        fgets(command, MAX_LINE, stdin);        // Nhập câu lệnh
        command[strcspn(command, "\n")] = '\0'; // strcspn tìm vị trí đầu tiên có '\n' trong chuỗi; '\0' kết thúc chuỗi tại đó. // Loại bỏ ký tự xuống dòng ('\n') ở cuối chuỗi nhập vào

        // Bỏ qua nếu câu lệnh rỗng
        if (strlen(command) == 0)
            continue;
        // Kiểm tra lệnh đặc biệt của người dùng nhập vào như "exit"
        else if (strcmp(command, "exit") == 0)
        {
            should_run = 0;
            continue;
        }
        // Nếu lệnh là "HF"
        else if (strcmp(command, "HF") == 0)
        {
            // In ra 5 lệnh gần nhất
            for (int i = 0; i < historyCount; i++)
            {
                printf("%s\n", HF[i]);
            }
            continue;
        }

        // Lưu lịch sử lại câu lệnh vừa nhập
        if (historyCount < HISTORY_SIZE)
        {
            strcpy(HF[historyCount], command);
            historyCount++;
        }
        // Nếu lịch sử đã đầy
        else
        {
            // Xóa lệnh đầu tiên và dịch chuyển các lệnh cũ lên trên
            for (int i = 0; i < HISTORY_SIZE - 1; i++)
            {
                strcpy(HF[i], HF[i + 1]);
            }
            strcpy(HF[HISTORY_SIZE - 1], command); // Lưu lệnh mới nhất vào cuối
        }

        pid = fork();
        if (pid == -1)
        {
            perror("fork() failed. Try again.");
            continue;
        }

        // Tiến trình con
        if (pid == 0)
        {
            childRunning = 1;
            char *cmd_list[MAX_COMMAND]; // Mảng để chứa các câu lệnh phân tách bằng dấu '|'. Ví dụ ["wc- l", "ls", "cat output.txt"]
            int cmd_count = 0;

            // Cấp phát bộ nhớ
            for (int i = 0; i < MAX_COMMAND; i++)
            {
                cmd_list[i] = malloc(MAX_LINE);
            }

            // Khúc này là để tách chuỗi command thành nhiều sub-command bằng dấu '|'
            char *token = strtok(command, "|");
            while (token != NULL && cmd_count < MAX_COMMAND)
            {
                // Remove leading and trailing spaces from the token
                while (*token == ' ' || *token == '\t')
                {
                    token++;
                }

                int length = strlen(token);
                while (length > 0 && (token[length - 1] == ' ' || token[length - 1] == '\t'))
                {
                    token[length - 1] = '\0';
                    length--;
                }

                strcpy(cmd_list[cmd_count++], token); // lưu subcommand vào câu lệnh cmd_list
                token = strtok(NULL, "|");     // tìm subcommand tiếp theo
            }

            // Khúc này là để chạy từng sub-command. Nếu có nhiều hơn 1 sub-command thì cơ chế pipe sẽ có hiệu lực
            int prevfd = STDIN_FILENO;
            for (int i = 0; i < cmd_count; i++)
            {
                int pipefd[2];
                if (pipe(pipefd) == -1)
                {
                    // Xử lý lỗi khi không thể tạo đường ống
                    perror("Pipe fail. Try again");
                    continue;
                }

                pid_t cpid = fork(); // Đẻ ra tiến trình con để chạy từng cmd trong trong cmd_list[]
                if (cpid == -1)
                {
                    perror("fork() failed in pipeline. Try again.");
                    exit(EXIT_FAILURE);
                }
                // Tiến trình con
                else if (cpid == 0)
                {
                    close(pipefd[0]);
                    dup2(prevfd, STDIN_FILENO);
                    if (i < cmd_count - 1)
                        dup2(pipefd[1], STDOUT_FILENO); // Chuyển hướng đầu ra của tất cả cmd trừ cmd cuối
                    exec_cmd(cmd_list[i]);
                    close(pipefd[1]);
                    exit(EXIT_SUCCESS); // Tiến trình con hi sinh sau khi chạy xong cmd
                }
                // Tiến trình cha
                else
                {
                    close(pipefd[1]);
                    close(prevfd);
                    prevfd = pipefd[0];
                    waitpid(-1, NULL, 0); // Chờ tiến trình con hi sinh
                }
            }

            childRunning = 0;
            for (int i = 0; i < MAX_COMMAND; i++) {
                free(cmd_list[i]); // Free bộ nhớ
            }
            exit(EXIT_SUCCESS);
        }

        // Tiến trình cha
        else
        {
            waitpid(-1, NULL, 0); // Chờ tiến trình con thực thi xong
        }
    }

    // Kết thúc
    for (int i = 0; i < historyCount; i++)
        free(HF[i]);
    for (int i = 0; i < argsCount; i++)
        free(args[i]);
    printf("Goodbye!");
    return 0;
}
