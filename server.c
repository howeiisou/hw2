#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int client_cnt = 4;

static const char MESSAGE[] = "Hello, World!\n";

static const int PORT = 9876;

static void listener_cb(struct evconnlistener *, evutil_socket_t,
                        struct sockaddr *, int socklen, void *);

static void conn_readcb(struct bufferevent *, void *);
// static void conn_writecb(struct bufferevent *, void *);
// static void conn_eventcb(struct bufferevent *, short, void *);
// static void signal_cb(evutil_socket_t, short, void *);

const int listenBacklog = 10;
char client_account[5][5] = {"usr1\0", "usr2\0", "usr3\0", "usr4\0"};
char client_password[5][5] = {"usr1\0", "usr2\0", "usr3\0", "usr4\0"};
int client_id[5] = {-1, -1, -1, -1, -1};
int client_stat[5] = {0, 0, 0, 0, 0}; //0:free,1:handle invite,2:gaming
int client_score[5] = {0};

int game_stat[2][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}};

int is_online(evutil_socket_t fd)
{
    int from = -1;
    for (int i = 0; i < client_cnt; i++)
    {
        if (fd == client_id[i])
        {
            from = i;
        }
    }
    return from;
}
int main(int argc, char **argv)
{
    int port = 9876;
    struct sockaddr_in my_address;
    memset(&my_address, 0, sizeof(my_address));
    my_address.sin_family = AF_INET;
    my_address.sin_port = htons(port);

    struct event_base *base = event_base_new();
    struct evconnlistener *listener = evconnlistener_new_bind(base, listener_cb,
                                                              NULL, LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1,
                                                              (struct sockaddr *)&my_address, sizeof(my_address));

    if (!listener)
        exit(1);

    event_base_dispatch(base);
    evconnlistener_free(listener);
    event_base_free(base);

    return 0;
}

//接收新连接的回调函数
static void listener_cb(struct evconnlistener *listener, evutil_socket_t fd,
                        struct sockaddr *sa, int socklen, void *user_data)
{
    printf("connection No.%d\n", fd);
    // set TCP_NODELAY
    // if (cli_cnt < 4)
    int enable = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&enable, sizeof(enable)) < 0)
        printf("Consensus-side: TCP_NODELAY SETTING ERROR!\n");

    struct event_base *base = evconnlistener_get_base(listener);
    struct bufferevent *new_buff_event = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    // bufferevent_setwatermark(new_buff_event, EV_READ, 72, 0);
    bufferevent_setcb(new_buff_event, conn_readcb, NULL, NULL, NULL);
    // set a read timeout of 1000 us
    // struct timeval tv = {0, 10000};
    // bufferevent_set_timeouts(new_buff_event, &tv, NULL);
    bufferevent_enable(new_buff_event, EV_READ | EV_WRITE);

    return;
}

static void
conn_readcb(struct bufferevent *bev, void *user_data)
{
    struct evbuffer *input = bufferevent_get_input(bev);
    size_t len = evbuffer_get_length(input);

    char *buf = (char *)malloc(sizeof(char) * len);
    if (NULL == buf)
        return;
    evbuffer_remove(input, buf, len);
    evutil_socket_t fd = bufferevent_getfd(bev);
    printf("%s", buf);

    if (strncmp(buf, "account:", 8) == 0)
    {
        char *ptr = strtok(buf, ","), *qtr = strtok(NULL, "\n");
        int acn_len = strlen(ptr) - 8, pwd_len = strlen(qtr) - 7;
        char acn[128], pwd[128];
        int i;

        strncpy(acn, buf + 8, acn_len);
        strncpy(pwd, ptr + 8, pwd_len);
        acn[acn_len] = '\0';
        pwd[pwd_len] = '\0';

        printf("%s\n%s\n", acn, pwd);

        int flag = 0;
        for (i = 0; i < client_cnt; i++)
        {
            if (client_id[i] == -1)
            {
                if (strcmp(client_account[i], acn) == 0 && strcmp(client_password[i], pwd) == 0)
                {
                    client_id[i] = (int)fd;
                    flag = 1;
                    break;
                }
            }
        }
        if (flag == 1)
        {
            char str[10] = "hi ";
            bufferevent_write(bev, strcat(str, acn), 7);
        }
        else
        {
            bufferevent_write(bev, "fail", 4);
        }
    }
    else if (strncmp(buf, "sign up:", 8) == 0)
    {
        char *ptr = strtok(buf, ","), *qtr = strtok(NULL, "\n");
        int acn_len = strlen(ptr) - 16, pwd_len = strlen(qtr) - 7;
        char acn[128], pwd[128];

        strncpy(acn, ptr + 16, acn_len);
        strncpy(pwd, qtr + 7, pwd_len);
        acn[acn_len] = '\0';
        pwd[pwd_len] = '\0';

        strcpy(client_account[4], acn);
        strcpy(client_password[4], pwd);

        client_cnt++;

        bufferevent_write(bev, "success", 7);
    }
    else if (strcmp(buf, "ls\n") == 0)
    {
        if (is_online(fd) != -1)
        {
            char idlist[1024] = {0};
            for (int i = 0; i < client_cnt; i++)
            {
                if (client_id[i] != -1)
                {
                    char tmp[128];
                    sprintf(tmp, "%s\t%d\n", client_account[i], client_score[i]);
                    strcat(idlist, tmp);
                }
            }
            bufferevent_write(bev, idlist, strlen(idlist));
        }
        else
        {
            bufferevent_write(bev, "login first", 11);
        }
    }
    else if (strncmp("invite:", buf, 7) == 0)
    {
        if (buf[strlen(buf) - 1] == '\n')
            buf[strlen(buf) - 1] = '\0';

        char *ptr = strtok(buf + 7, "\n");
        int from = is_online(fd);

        printf("invite name %s\n", ptr);

        if (from == -1)
        {
            bufferevent_write(bev, "please login first", 18);
        }
        else
        {
            int opponent = -1;
            for (int i = 0; i < client_cnt; i++)
            {
                if (client_id[i] != -1 && strcmp(ptr, client_account[i]) == 0)
                    opponent = i;
            }

            if (client_stat[opponent] != 0 && opponent != -1)
            {
                bufferevent_write(bev, "player is busy", 14);
            }
            else if (opponent != -1)
            {
                char tmp[1024] = "invite msg from:";
                int tmplen = strlen(client_account[from]) + strlen(tmp);
                client_stat[opponent] = 1;
                client_stat[from] = 1;
                write(client_id[opponent], strcat(tmp, client_account[from]), tmplen);
            }
            else
            {
                bufferevent_write(bev, "player does not exist", 21);
            }
        }
    }
    else if (strncmp("answer:", buf, 7) == 0)
    {
        // if (buf[strlen(buf) - 1] == '\n')
        //     buf[strlen(buf) - 1] = '\0';

        char *target_name = strtok(buf + 8, "\n");
        char reply = buf[7];
        int opponent, from = is_online(fd);
        int game_id;

        for (int i = 0; i < client_cnt; i++)
        {
            if (strcmp(target_name, client_account[i]) == 0)
                opponent = i;
        }

        if (reply == 'y')
        {
            if (game_stat[0][0] == 0)
                game_id = 0;
            else
                game_id = 1;

            client_stat[opponent] = 2;
            client_stat[from] = 2;

            game_stat[game_id][0] = 1;
            game_stat[game_id][1] = opponent;
            game_stat[game_id][2] = from;
            game_stat[game_id][3] = from;

            write(client_id[opponent], "start\n", 6);
            bufferevent_write(bev, "start\nit's your turn\n", 21);
        }
        else if (reply == 'n')
        {
            client_stat[opponent] = 0;
            client_stat[from] = 0;
            write(client_id[opponent], "invite is rejected", 18);
        }
    }
    else if (strncmp("at:", buf, 3) == 0)
    {
        int game_id;
        int from = is_online(fd);

        if (game_stat[0][1] == from || game_stat[0][2] == from)
        {
            game_id = 0;
        }
        else
        {
            game_id = 1;
        }

        if (game_stat[game_id][3] != from)
        {
            bufferevent_write(bev, "not your turn!", 14);
        }
        else
        {
            if (game_stat[game_id][1] == from)
            {
                game_stat[game_id][3] = game_stat[game_id][2];
            }
            else
            {
                game_stat[game_id][3] = game_stat[game_id][1];
            }
            write(client_id[game_stat[game_id][3]], buf, strlen(buf));
            char str[10] = "done:";
            strcat(str, buf);
            bufferevent_write(bev, str, strlen(str));
        }
    }
    else if (strncmp("logout", buf, 6) == 0)
    {
        int from = is_online(fd);
        client_id[from] = -1;

        bufferevent_write(bev, "logout success", 14);
    }
    else if (strncmp("nextgame", buf, 8) == 0 || strstr(buf, "surrender") != NULL || strstr(buf, "win") != NULL)
    {
        int game_id;
        int from = is_online(fd);

        if (game_stat[0][1] == from || game_stat[0][2] == from)
        {
            game_id = 0;
        }
        else
        {
            game_id = 1;
        }

        int opponent = game_stat[game_id][1] == from ? game_stat[game_id][2] : game_stat[game_id][1];
        client_stat[game_stat[game_id][1]] = 0;
        client_stat[game_stat[game_id][2]] = 0;
        for (int i = 0; i < client_cnt; i++)
        {
            game_stat[game_id][i] = 0;
        }

        if (strncmp("surrender", buf, 9) == 0)
        {
            client_score[from]--;
            bufferevent_write(bev, "you surrendered", 15);
            write(client_id[opponent], "opponent surrendered", 20);
        }
        else
        {
            client_score[from]++;
            bufferevent_write(bev, "table inited", 12);
            write(client_id[opponent], "table inited", 12);
        }
    }
    else if (strncmp("pm", buf, 2) == 0)
    {
        // if (buf[strlen(buf) - 1] == '\n')
        //     buf[strlen(buf) - 1] = '\0';
        char *ptr = strtok(buf + 3, " ");
        char *str = strtok(NULL, "\n");

        int from = is_online(fd);

        if (from == -1)
        {
            bufferevent_write(bev, "please login first", 18);
        }
        else
        {
            int opponent = -1;
            for (int i = 0; i < client_cnt; i++)
            {
                if (client_id[i] != -1 && strcmp(ptr, client_account[i]) == 0)
                    opponent = i;
            }
            if (opponent != -1)
            {
                char tmp[1024] = "pm from:";
                strcat(tmp, client_account[from]);
                strcat(tmp, "\n");
                strcat(tmp, str);
                write(client_id[opponent], tmp, strlen(tmp));
            }
            else
            {
                bufferevent_write(bev, "player does not exist", 21);
            }
        }
    }
    else
    {
        bufferevent_write(bev, "sorry", 5);
    }

    memset(buf, '\0', sizeof(buf));
    free(buf);
    buf = NULL;
    return;
}