#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>

static const int PORT = 9876;
const char *server_ip = "127.0.0.1";

char table[] = {"___|___|___\n___|___|___\n   |   |   \n\n"};
char table_init[] = {"_1_|_2_|_3_\n_4_|_5_|_6_\n 7 | 8 | 9 \n\n"};

char mark = 'o';

int is_win()
{
    int flag = 0;

    if (table[1] == table[5] && table[5] == table[9] && table[1] != '_')
        flag = 1;

    if (table[13] == table[17] && table[17] == table[21] && table[13] != '_')
        flag = 1;

    if (table[25] == table[29] && table[29] == table[33] && table[25] != ' ')
        flag = 1;

    if (table[1] == table[13] && table[13] == table[25] && table[1] != '_')
        flag = 1;

    if (table[5] == table[17] && table[17] == table[29] && table[5] != '_')
        flag = 1;

    if (table[9] == table[21] && table[21] == table[33] && table[9] != '_')
        flag = 1;

    if (table[1] == table[17] && table[17] == table[33] && table[1] != '_')
        flag = 1;

    if (table[9] == table[17] && table[17] == table[25] && table[9] != '_')
        flag = 1;

    return flag;
}
/*事件处理回调函数*/
void event_cb(struct bufferevent *bev, short events, void *ptr)
{
    if (events & BEV_EVENT_CONNECTED) //连接建立成功
    {
        printf("connected to server successed!\r\n");
    }
    else if (events & BEV_EVENT_ERROR)
    {
        printf("connect error happened!");
    }
}

void read_cb(struct bufferevent *bev, void *ctx)
{
    struct evbuffer *input = bufferevent_get_input(bev);
    size_t len = 0;
    len = evbuffer_get_length(input);
    // read
    char *buf;
    buf = (char *)malloc(sizeof(char) * len);
    if (NULL == buf)
    {
        return;
    }
    evbuffer_remove(input, buf, len);

    printf("%s\n", buf);

    if (strncmp(buf, "at:", 3) == 0)
    {
        char opponent_mark = (mark == 'o' ? 'x' : 'o');
        int pos = 4 * (buf[3] - '1') + 1;
        table[pos] = opponent_mark;
        printf("%s", table);

        if (is_win() != 0)
        {
            printf("you lose!\n");
            int i;
            for (i = 1; i <= 21; i += 4)
            {
                table[i] = '_';
            }
            for (; i <= 33; i += 4)
            {
                table[i] = ' ';
            }
            mark = 'o';
        }
    }

    else if (strstr(buf, "start") != NULL)
    {
        printf("%s", table_init);
    }

    else if (strstr(buf, "invite msg") != NULL)
    {
        mark = 'x';
    }

    else if (strncmp(buf, "done:", 5) == 0)
    {
        int pos = 4 * (buf[8] - '1') + 1;
        table[pos] = mark;
        printf("%s", table);

        if (is_win() != 0)
        {
            printf("you win!\n");
            int i;
            for (i = 1; i <= 21; i += 4)
            {
                table[i] = '_';
            }
            for (; i <= 33; i += 4)
            {
                table[i] = ' ';
            }
            mark = 'o';
            bufferevent_write(bev, "win", 3);
        }
    }

    else if (strstr(buf, "surrender") != NULL)
    {
        int i;
        for (i = 1; i <= 21; i += 4)
        {
            table[i] = '_';
        }
        for (; i <= 33; i += 4)
        {
            table[i] = ' ';
        }
        mark = 'o';
    }
}

int tcp_connect_server(const char *server_ip, int port)
{
    struct sockaddr_in server_addr;
    int status = -1;
    int sockfd;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    status = inet_aton(server_ip, &server_addr.sin_addr);
    if (0 == status)
    {
        errno = EINVAL;
        return -1;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
        return sockfd;

    status = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    if (status == -1)
    {
        close(sockfd);
        return -1;
    }

    evutil_make_socket_nonblocking(sockfd);

    return sockfd;
}

void cmd_msg_cb(int fd, short events, void *arg)
{
    char msg[1024];
    memset(msg, '\0', sizeof(msg));

    int ret = read(fd, msg, sizeof(msg));
    if (ret < 0)
    {
        perror("read failed");
        exit(1);
    }

    struct bufferevent *bev = (struct bufferevent *)arg;

    msg[ret] = '\0';
    //把终端消息发给服务器段
    bufferevent_write(bev, msg, strlen(msg));

    printf("send message %s", msg);
}

int main()
{
    struct event_base *base = NULL;
    struct sockaddr_in server_addr;
    struct bufferevent *bev = NULL;
    int status;
    int sockfd;

    //申请event_base对象
    base = event_base_new();
    if (!base)
    {
        printf("event_base_new() function called error!");
    }

    //初始化server_addr
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    status = inet_aton(server_ip, &server_addr.sin_addr);

    bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE); //第二个参数传-1,表示以后设置文件描述符

    //调用bufferevent_socket_connect函数
    bufferevent_socket_connect(bev, (struct sockaddr *)&server_addr, sizeof(server_addr));

    //监听终端的输入事件
    struct event *ev_cmd = event_new(base, STDIN_FILENO, EV_READ | EV_PERSIST, cmd_msg_cb, (void *)bev);

    //添加终端输入事件
    event_add(ev_cmd, NULL);

    //设置bufferevent各回调函数
    bufferevent_setcb(bev, read_cb, NULL, event_cb, (void *)NULL);

    //启用读取或者写入事件
    bufferevent_enable(bev, EV_READ | EV_PERSIST);

    //开始事件管理器循环
    event_base_dispatch(base);

    event_base_free(base);
    return 0;
}