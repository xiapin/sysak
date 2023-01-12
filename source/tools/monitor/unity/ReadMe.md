# 构建编译镜像

从Dockerfile 构建镜像，最新git路径 `https://gitee.com/chuyansz/sysak/blob/opensource_branch/source/tools/monitor/unity/Dockerfile`

构建环境需要以下准备：

1. 安装了docker
2. 可以访问gitee和github

执行以下命令构建
```
docker build -c 2 -t sysom:v1.1 .
```

大约10+分钟后即可构建完毕

# 使用编译镜像

如果在本地构建完毕，可以采用以下命令拉起刚才构建好的镜像

```bash
docker run -itd --net=host --name sysom-devel sysom:v1.1
docker exec -it unity-devel bash
```

如果采用已经好的镜像

```bash
docker run -itd --net=host --name sysom-devel registry.cn-hangzhou.aliyuncs.com/sysom/sysom
docker exec -it sysom-devel bash
```

进入到容器后，进入到代码路径，执行启动脚本即可拉起监控服务

```bash
cd build/sysak/source/tools/monitor/unity/test/bees/
./run.sh
```

通过访问8400 端口即可获取到数据，也可以用浏览器浏览页面

```bash
curl 127.0.0.1:8400
```