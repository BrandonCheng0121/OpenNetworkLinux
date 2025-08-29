#include <unistd.h>
#include <fcntl.h>
#include "platform_lib.h"
#include <onlp/platformi/sfpi.h>
#include "x86_64_muxi_dcs6500_48z8c_log.h"

int onlp_file_write_integer(char *filename, int value)
{
    char buf[8] = {0};
    sprintf(buf, "%d", value);

    return onlp_file_write((uint8_t*)buf, strlen(buf), filename);
}

int onlp_file_read_binary(char *filename, char *buffer, int buf_size, int data_len)
{
    int fd;
    int len;

    if ((buffer == NULL) || (buf_size < 0)) {
        return -1;
    }

    if ((fd = open(filename, O_RDONLY)) == -1) {
        return -1;
    }

    if ((len = read(fd, buffer, buf_size)) < 0) {
        close(fd);
        return -1;
    }

    if ((close(fd) == -1)) {
        return -1;
    }

    if ((len > buf_size) || (data_len != 0 && len != data_len)) {
        return -1;
    }

    return 0;
}

int onlp_file_read_string(char *filename, char *buffer, int buf_size, int data_len)
{
    int ret;

    if (data_len >= buf_size) {
	    return -1;
	}

	ret = onlp_file_read_binary(filename, buffer, buf_size-1, data_len);

    if (ret == 0) {
        buffer[buf_size-1] = '\0';
    }

    return ret;
}

int _s3ip_int_node_get(int id, char *name, char *node, int *value)
{
    int ret = ONLP_STATUS_OK;
    char path[ONLP_CONFIG_INFO_STR_MAX] = {0};
    sprintf(path, "/sys/s3ip_switch/%s/%s%d/%s", name, name, id, node);
    DEBUG_PRINT("%s(%d), %s path = (%s)", name, id, node, path);

    if (onlp_file_read_int(value, path) < 0)
    {
        AIM_LOG_ERROR("Unable to read %s from file (%s)\r\n", node, path);
        return ONLP_STATUS_E_INTERNAL;
    }
    return ret;
}

int _s3ip_str_node_get(int id, char *name, char *node, char *str)
{
    int ret = ONLP_STATUS_OK;
    char *string = NULL;
    int len = 0;
    char path[ONLP_CONFIG_INFO_STR_MAX] = {0};
    sprintf(path, "/sys/s3ip_switch/%s/%s%d/%s", name, name, id, node);
    DEBUG_PRINT("%s(%d), %s path = (%s)", name, id, node, path);
    len = onlp_file_read_str(&string, path);
    if (string && len)
    {
        if (len >= (ONLP_CONFIG_INFO_STR_MAX - 1))
        {
            len = ONLP_CONFIG_INFO_STR_MAX - 1;
        }
        memcpy(str, string, len);
        str[len] = '\0';
    }
    else
    {
        AIM_LOG_ERROR("Unable to read %s from file (%s)\r\n", node, path);
        ret = ONLP_STATUS_E_INTERNAL;
    }
    AIM_FREE_IF_PTR(string);
    return ret;
}

int s3ip_int_path_get(int id, char *abs_path, char *node, int *value)
{
    int ret = ONLP_STATUS_OK;
    char path[ONLP_CONFIG_INFO_STR_MAX] = {0};
    sprintf(path, "%s%d/%s", abs_path, id, node);
    DEBUG_PRINT("%s(%d), %s path = (%s)", abs_path, id, node, path);

    if (onlp_file_read_int(value, path) < 0)
    {
        AIM_LOG_ERROR("Unable to read %s from file (%s)\r\n", node, path);
        return ONLP_STATUS_E_INTERNAL;
    }
    return ret;
}

int s3ip_int_path_set(int id, char *abs_path, char *node, int value)
{
    int ret = ONLP_STATUS_OK;
    char path[ONLP_CONFIG_INFO_STR_MAX] = {0};
    sprintf(path, "%s%d/%s", abs_path, id, node);
    DEBUG_PRINT("eth(%d), %s path = (%s)", id, node, path);
    if (onlp_file_write_integer(path, value) < 0)
    {
        AIM_LOG_ERROR("Unable to write data to file (%s)\r\n", path);
        return ONLP_STATUS_E_INTERNAL;
    }
    return ret;
}

int s3ip_str_path_get(int id, char *abs_path, char *node, char *str)
{
    int ret = ONLP_STATUS_OK;
    char *string = NULL;
    int len = 0;
    char path[ONLP_CONFIG_INFO_STR_MAX] = {0};
    sprintf(path, "%s%d/%s", abs_path, id, node);
    DEBUG_PRINT("%s(%d), %s path = (%s)", abs_path, id, node, path);
    len = onlp_file_read_str(&string, path);
    if (string && len)
    {
        if (len >= (ONLP_CONFIG_INFO_STR_MAX - 1))
        {
            len = ONLP_CONFIG_INFO_STR_MAX - 1;
        }
        memcpy(str, string, len);
        str[len] = '\0';
    }
    else
    {
        AIM_LOG_ERROR("Unable to read %s from file (%s)\r\n", node, path);
        ret = ONLP_STATUS_E_INTERNAL;
    }
    AIM_FREE_IF_PTR(string);
    return ret;
}