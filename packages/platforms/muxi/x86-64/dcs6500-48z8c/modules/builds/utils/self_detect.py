#!/usr/bin/python3
from distutils.fancy_getopt import fancy_getopt
import json
import os
import subprocess
import self_detect.psu_test
import self_detect.fan_test
import self_detect.sensor_test
import self_detect.sys_cmd_test

test_error_list = []
def call_retcode(cmd):
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, shell=True)
    outs = p.communicate()
    return outs[0].decode()+outs[1].decode()

def data_type_handle(data_type, source_type, path_cmd, enmu_def_dict):
    if(source_type == 'node'):
        if('bit' in data_type):
            print('cat '+path_cmd)
            ret_str = call_retcode('cat '+path_cmd)
            print(ret_str)
            if 'No such' in ret_str:
                print(path_cmd+' non-existent')
                test_error_list.append(path_cmd+' non-existent')
                return False
            if 'NA' == ret_str.strip('\n'):
                print(path_cmd+' return NA')
                test_error_list.append(path_cmd+' return NA')
                return False
            # print(len(enmu_def_dict[data_type]))
            bit_num = len(enmu_def_dict[data_type])
            for bit_n in range(0, bit_num):
                val_bit = enmu_def_dict[data_type][bit_n]['val_bit']
                # print(enmu_def_dict[data_type][bit_n]['val_bit'])
                if(int(ret_str) >> int(val_bit) & 1):
                    print(enmu_def_dict[data_type][bit_n]['details'])
        elif('enmu' in data_type):
            print('cat '+path_cmd)
            ret_str = call_retcode('cat '+path_cmd)
            print(ret_str)
            if 'No such' in ret_str:
                print(path_cmd+' non-existent')
                test_error_list.append(path_cmd+' non-existent')
                return False
            if 'NA' == ret_str.strip('\n'):
                print(path_cmd+' return NA')
                test_error_list.append(path_cmd+' return NA')
                return False
            enmu_num = len(enmu_def_dict[data_type])
            enmu_def_dict[data_type]
            for enmu_n in range(0, enmu_num):
                val = enmu_def_dict[data_type][enmu_n]['val']
                expect = enmu_def_dict[data_type][enmu_n]['expect']
                # print(enmu_def_dict[data_type][enmu_n]['val'])
                if 'NA' not in ret_str:
                    if(expect != "none"):
                        if( int(ret_str) == int(val) ):
                            print(enmu_def_dict[data_type][enmu_n]['details'])
                            if( int(ret_str) != int(expect) ):
                                print("the node value not as expected\n"+' expected '+expect+'\n')
                                test_error_list.append(path_cmd+' the node value not as expected')
                                return False
        elif('hex' in data_type):
            print('hexdump -C '+path_cmd)
            ret_str = call_retcode('hexdump -C '+path_cmd)
            print(ret_str)
            if 'No such' in ret_str :
                print(path_cmd+' non-existent')
                test_error_list.append(path_cmd+' non-existent')
                return False
            if 'NA' == ret_str.strip('\n'):
                print(path_cmd+' return NA')
                test_error_list.append(path_cmd+' return NA')
                return False
            if 'Connection timed out'in ret_str:
                if("/eeprom" in path_cmd):
                    new_path_cmd = path_cmd.replace('/eeprom', '/present')
                    ret_str = call_retcode('cat '+new_path_cmd)
                    if(ret_str.strip('\n') == '1'):
                        print(path_cmd+' error')
                        test_error_list.append(path_cmd+' error')
                        return False
        elif('const' in data_type):
            print('cat '+path_cmd)
            ret_str = call_retcode('cat '+path_cmd)
            print(ret_str)
            if 'No such' in ret_str:
                print(path_cmd+' non-existent')
                test_error_list.append(path_cmd+' non-existent')
                return False
            if 'NA' == ret_str.strip('\n'):
                print(path_cmd+' return NA')
                test_error_list.append(path_cmd+' return NA')
                return False
            const_val = enmu_def_dict[data_type]['val']
            if(const_val != ret_str.strip('\n')):
                print(enmu_def_dict[data_type]['err_details']+' expected '+const_val)
                test_error_list.append(path_cmd+' the node value not as expected')
                return False
        elif('count' in data_type) or ('none' in data_type) or ('path' in data_type):
            pass
        elif('na' in data_type):
            ret_str = call_retcode('cat '+path_cmd)
            print(ret_str)
            if 'NA' not in ret_str:
                print(path_cmd+' does not return NA as expected')
                test_error_list.append(path_cmd+' data_type is NA, but return value is not.')
                return False
        else: #string int float
            print('cat '+path_cmd)
            ret_str = call_retcode('cat '+path_cmd)
            print(ret_str)
            if 'No such' in ret_str:
                print(path_cmd+' non-existent')
                test_error_list.append(path_cmd+' non-existent')
                return False
            if 'NA' == ret_str.strip('\n'):
                print(path_cmd+' return NA')
                test_error_list.append(path_cmd+' return NA')
                return False

    elif(source_type == 'cmd'):
        if('bit' in data_type):
            ret_str = call_retcode(path_cmd)
            bit_num = len(enmu_def_dict[data_type])
            for bit_n in range(0, bit_num):
                val_bit = enmu_def_dict[data_type][bit_n]['val_bit']
                if(int(ret_str) >> int(val_bit) & 1):
                    print(enmu_def_dict[data_type][bit_n]['details'])
        elif('enmu' in data_type):
            ret_str = call_retcode(path_cmd)
            enmu_num = len(enmu_def_dict[data_type])
            enmu_def_dict[data_type]
            for enmu_n in range(0, enmu_num):
                val = enmu_def_dict[data_type][enmu_n]['val']
                expect = enmu_def_dict[data_type][enmu_n]['expect']
                # print(enmu_def_dict[data_type][enmu_n]['val'])
                if( int(ret_str) == int(val) ):
                    print(enmu_def_dict[data_type][enmu_n]['details'])
                    if( int(ret_str) != int(expect) ):
                        print("the node value not as expected")
        elif('hex' in data_type):
            call_retcode(path_cmd)
        elif('none' in data_type):
            pass
        else:#string int float
            call_retcode(path_cmd)
    elif(source_type == 'path'):
        pass
    return True

def test_main():
    test_pass = True
    cur_num1 = -1
    cur_num2 = -1
    cur_num3 = -1
    cur_num4 = -1
    cur_num5 = -1
    run_count1 = 0
    run_count2 = 0
    run_count3 = 0
    run_count4 = 0
    run_count5 = 0
    # 读取json数据获取所有节点信息
    with open('/usr/local/bin/self_detect/kis_platform_table.json', 'r') as f:
    # with open('abc.json', 'r') as f:
        kis_json_str = json.load(f)

    # 获取所有常量
    enmu_def_keys = kis_json_str.keys()
    enmu_def_dict = kis_json_str

    node_layer0_keys = kis_json_str['kis'].keys()  # 解第零层
    node_layer0_dict = kis_json_str['kis']
    source_type0 = node_layer0_dict['source_type']
    if(source_type0 == 'cmd'):
        path_cmd0 = node_layer0_dict['path_cmd']
    elif(source_type0 == 'path') or (source_type0 == 'node'):
        path_cmd0 = node_layer0_dict['path_cmd']
    data_type0 = node_layer0_dict['data_type']
    print("key0")
    print(node_layer0_keys)

    for node_layer1 in node_layer0_keys:  # 解第一层 syseeprom fan psu ...
        try:
            node_layer1_keys = node_layer0_dict[node_layer1].keys()
            node_layer1_dict = node_layer0_dict[node_layer1]
        except:
            continue
        print ("\n\n***********kis node {} test***********\n".format(node_layer1))
        source_type1 = node_layer1_dict['source_type']
        if(source_type1 == 'cmd'):
            path_cmd1 = node_layer1_dict['path_cmd']
        elif(source_type1 == 'path') or (source_type1 == 'node'):
            path_cmd1 = path_cmd0 + node_layer1_dict['path_cmd']
        data_type1 = node_layer1_dict['data_type']
        if(data_type_handle(data_type1, source_type1, path_cmd1, enmu_def_dict) == False):#判定源类型 和 数据类型 对应的执行方法
            test_pass = False
        # print("key1")
        # print(node_layer1_keys)
        run_count1 = 0
        cur_num1 = -1
        while True:
            val1 = 0
            if('count' in data_type1):
                val1 = int(enmu_def_dict[data_type1]['val'])
                start_num1 = int(enmu_def_dict[data_type1]['start_num'])
                if(cur_num1 == -1):
                    cur_num1 = int(start_num1)
                path_cmd1 = path_cmd0 + node_layer1_dict['path_cmd'] + str(cur_num1)
                cur_num1 = cur_num1 + 1
            if val1 > 0:
                if cur_num1 > (val1 + start_num1) or cur_num1 == -1:
                    break
            elif run_count1 >= 1:
                break
            run_count1 = run_count1 + 1

            for node_layer2 in node_layer1_keys:  # 解第二层
                try:
                    node_layer2_keys = node_layer1_dict[node_layer2].keys()
                    node_layer2_dict = node_layer1_dict[node_layer2]
                except:
                    continue
                source_type2 = node_layer2_dict['source_type']
                if(source_type2 == 'cmd'):
                    path_cmd2 = node_layer2_dict['path_cmd']
                elif(source_type2 == 'node') or (source_type2 == 'path'):
                    path_cmd2 = path_cmd1 + node_layer2_dict['path_cmd']
                data_type2 = node_layer2_dict['data_type']
                if(data_type_handle(data_type2, source_type2, path_cmd2, enmu_def_dict) == False):#判定源类型 和 数据类型 对应的执行方法
                    test_pass = False
                # print("key2")
                # print(node_layer2_keys)
                run_count2 = 0
                cur_num2 = -1
                while True:
                    val2 = 0
                    if('count' in data_type2):
                        val2 = int(enmu_def_dict[data_type2]['val'])
                        start_num2 = int(enmu_def_dict[data_type2]['start_num'])
                        if(cur_num2 == -1):
                            cur_num2 = start_num2
                        path_cmd2 = path_cmd1 + node_layer2_dict['path_cmd'] + str(cur_num2)
                        cur_num2 = cur_num2 + 1
                    if val2 > 0:
                        if cur_num2 > (val2 + start_num2) or cur_num2 == -1:
                            break
                    elif run_count2 >= 1:
                        break
                    run_count2 = run_count2 + 1

                    for node_layer3 in node_layer2_keys:  # 解第三层
                        try:
                            node_layer3_keys = node_layer2_dict[node_layer3].keys()
                            node_layer3_dict = node_layer2_dict[node_layer3]
                        except:
                            continue
                        source_type3 = node_layer3_dict['source_type']
                        if(source_type3 == 'cmd'):
                            path_cmd3 = node_layer3_dict['path_cmd']
                        elif(source_type3 == 'node') or (source_type3 == 'path'):
                            path_cmd3 = path_cmd2 + node_layer3_dict['path_cmd']
                        data_type3 = node_layer3_dict['data_type']
                        if(data_type_handle(data_type3, source_type3, path_cmd3, enmu_def_dict) == False):#判定源类型 和 数据类型 对应的执行方法
                            test_pass = False
                        # print("key3")
                        # print(node_layer3_keys)
                        run_count3 = 0
                        cur_num3 = -1
                        while True:
                            val3 = 0
                            if('count' in data_type3):
                                val3 = int(enmu_def_dict[data_type3]['val'])
                                start_num3 = int(enmu_def_dict[data_type3]['start_num'])
                                if(cur_num3 == -1):
                                    cur_num3 = start_num3
                                path_cmd3 = path_cmd2 + node_layer3_dict['path_cmd'] + str(cur_num3)
                                cur_num3 = cur_num3 + 1
                            if val3 > 0:
                                if cur_num3 > (val3 + start_num3) or cur_num3 == -1:
                                    break
                            elif run_count3 >= 1:
                                break
                            run_count3 = run_count3 + 1

                            for node_layer4 in node_layer3_keys:  # 解第四层
                                try:
                                    node_layer4_keys = node_layer3_dict[node_layer4].keys()
                                    node_layer4_dict = node_layer3_dict[node_layer4]
                                except:
                                    continue
                                source_type4 = node_layer4_dict['source_type']
                                if(source_type4 == 'cmd'):
                                    path_cmd4 = node_layer4_dict['path_cmd']
                                elif(source_type4 == 'node') or (source_type4 == 'path'):
                                    path_cmd4 = path_cmd3 + node_layer4_dict['path_cmd']
                                data_type4 = node_layer4_dict['data_type']
                                if(data_type_handle(data_type4, source_type4, path_cmd4, enmu_def_dict) == False):#判定源类型 和 数据类型 对应的执行方法
                                    test_pass = False
                                # print("key4")
                                # print(node_layer4_keys)


    test_error_list.append('=========================================================\n')
    test_error_list.append('=================SUMMARY OF TEST RESULTS=================\n')
    if(test_pass == False):
        test_error_list.append('kis node test: FAILED\n')
    else:
        test_error_list.append('all kis node test: PASSED\n')

    # 节点数据按格式输出
    fan_t = self_detect.fan_test.FAN_TEST()
    if(fan_t.run_test() != True):
        test_error_list.append('fan status test: FAILED\n')
    else:
        test_error_list.append('fan status test: PASSED\n')
    psu_t = self_detect.psu_test.PSU_TEST()
    if(psu_t.run_test() != True):
        test_error_list.append('psu status test: FAILED\n')
    else:
        test_error_list.append('psu status test: PASSED\n')
    sensor_t = self_detect.sensor_test.SENSOR_TEST()
    if(sensor_t.run_test() != True):
        test_error_list.append('sensor status test: FAILED\n')
    else:
        test_error_list.append('sensor status test: PASSED\n')
    syscmd_t = self_detect.sys_cmd_test.SYS_CMD_TEST()
    if(syscmd_t.run_test() != True):
        test_error_list.append('sys_cmd status test: FAILED\n')
    else:
        test_error_list.append('sys_cmd status test: PASSED\n')
    test_error_list.append('=========================================================\n')

    if(test_pass == False):
        print('test failed list:')
    for err_mesg in test_error_list:
        print(err_mesg)

test_main()
