# 驱动结构说明
third_party --存放所有第三方开源驱动和zb自己的底层直接沟通硬件的驱动
switch_drivers --用于数据处理转换的中间层接口
s3ip --用于s3ip+fmea节点生成

# 驱动数据流向说明
third_party --> switch_drivers --> s3ip

举例说明
third提供了lm75的温度读取接口get_lm75，switch_drivers通过调用get_lm75并将数据整理为需要的格式并归一化接口为get_temp(index)，s3ip建立节点与switch_drivers中接口的联系
结果就是：用户cat s3ip节点 s3ip调用了s3ip节点对应的s3ip_get_xxx，s3ip_get_xxx又调用switch_drivers中的get_xxx，get_xxx调用到third_party中的get_hardware_data


# 代码规范
1.所有switch_drivers和s3ip中的文件必须使用libboundscheck对应的安全接口，third_party中的代码我们新增的部分也必须使用安全接口，包括：
                sprintf_s            vsprintf_s
                sscanf_s             vsscanf_s
                strcat_s
                memcpy_s             strcpy_s
                memmove_s            strncat_s
                memset_s             strncpy_s
                strtok_s
                securecutil
                secureinput_a
                secureprintoutput_a
                snprintf_s           vsnprintf_s
2.switch_drivers层提供给s3ip层的接口，返回值有int型和char型，数据传递一律使用指针参数方式，形式如下：
    int attr_val = 0;
    ret = cb_func->get_status(fan_index, &attr_val);
    char buf[];
    ret = cb_func->get_sn(fan_index, &buf);