245 上开发

```
docker pull registry.cn-hangzhou.aliyuncs.com/sysom/unity:v1.1
docker run --net=host --privileged=true  -v /:/mnt/host:ro --name unity -it -d registry.cn-hangzhou.aliyuncs.com/sysom/unity:v1.1 /bin/bash
docker exec -it unity bash
cd build/sysak/source/tools/monitor/unity/test/bees/
./run.sh
```