//
// Created by 廖肇燕 on 2023/1/28.
//

#ifndef UNITY_SAFE_POPEN_H
#define UNITY_SAFE_POPEN_H

int safe_popen_read(const char *cmd, char *buff, int len, int timeout_ms);

#endif //UNITY_SAFE_POPEN_H
