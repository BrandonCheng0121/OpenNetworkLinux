source_type:
    node 节点类型会cat路径'path_cmd'，获取信息
    cmd 通过执行'path_cmd'指令获取，获取信息
data_type：
    xxx_enmu 会读取相应节点信息后与枚举值比较，并输出'details'相应信息，如果不符合期望值'expect'则报错
    xxx_bit 会读取数值相应'bit'位确认数值意义，并输出相应信息
    xxx_const 会比较'val'是否一样，不一样会报错,输出'err_details'为报错信息
    path 会将当前'source_type'路径作为'base'传递到下一级
    string 会输出相应字符串信息
    int 会输出相应数值信息
    float 会输出相应数值信息
    hex 会通过hexdump -C的方式替代cat获取信息
path_cmd:
    根据'source_type'类型填入路径或者指令

# 测试内容说明
## 需要检测的模块
所有驱动节点是否存在，可否进行读取
所有状态类节点值是否符合预期
## 测试项目
psu：所有电压电流功率是否范围内
fan：风扇速度是否范围内
sensor：所有电压 电流 温度 是否范围内

# 测试代码说明
## 节点测试通过配置json即可实现
    目前使用json解析嵌套for对json解析出所有节点路径，通过属性数据区分处理方法，比如syseeprom测试

    1. kis节点下，"data_type"为"path"，得到"base"路径，"/sys/switch"，进行下一层解析
    2. "syseeprom"节点下，"data_type"为"path"，得到"base"路径，"/sys/switch"+"/syseeprom"，进行下一层解析
    3. "help"节点下，"data_type"为"string"，"source_type"为"node"，程序自动通过"cat"方法得到节点内容并输出等效执行cat /sys/switch/syseeprom/debug
    4. "loglevel"节点下，"data_type"为"loglevel_bit"，"source_type"为"node"，程序自动通过"cat"方法得到节点内容，根据"loglevel_bit"定义，比较每个bit的值，并输出相应的打印信息
    5. "eeprom"节点下，"data_type"为"hex"，"source_type"为"node"，程序自动通过"hexdump"方法得到节点内容，并输出相应的打印信息
    6. "bsp_version"节点下，"data_type"为"bsp_version_const"，"source_type"为"node"，程序自动通过"cat"方法得到节点内容，并与"bsp_version_const"的值比较，不同则输出相应的"err_details"

    "bsp_version_const":
        {"val":"0.0.3","err_details":"Driver version is no match "},
    "loglevel_bit":[
        {"val_bit":"0","details":"log level is \"error\""},
        {"val_bit":"1","details":"log level is \"warning\""},
        {"val_bit":"2","details":"log level is \"info\""},
        {"val_bit":"3","details":"log level is \"debug\""}
    ],

    "kis": {
    "source_type":"node",
    "path_cmd":"/sys/switch",
    "data_type":"path",
    "syseeprom": {
            "source_type":"node",
            "path_cmd":"/syseeprom",
            "data_type":"path",
            "help":{
                "source_type":"node",
                "path_cmd":"/debug",
                "data_type":"string"
            },
            "loglevel": {
                "source_type":"node",
                "path_cmd":"/loglevel",
                "data_type":"loglevel_bit"
            },
            "eeprom": {
                "source_type":"node",
                "path_cmd":"/eeprom",
                "data_type":"hex"
            },
            "bsp_version": {
                "source_type":"node",
                "path_cmd":"/bsp_version",
                "data_type":"bsp_version_const"
            }
        }
    }

## 模块的 "参数/功能" 测试需要通过相应的py文件实现
    提供run_test()方法自动完成测试，相应的py代码请根据实际需要进行维护