模块功能：
	将prometheus格式字符串转化为snappy压缩后的字节流
 
run：
	sh ./build_shared.sh
  luajit ffi_lua.lua

test:
  修改metricstore信息
  go run test_sls.go