# web 页面开发

监控通过markdown引擎对页面进行渲染，因此开发者只需要编写markdown文档，修改下脚本，触发下热更新就可以了。

## 编写markdown

关于markdown，可以参考这个[教程](https://www.runoob.com/markdown/md-tutorial.html)。**需要注意的是**， 当前引擎还不支持**附件**，因此纯文本创作就好了。

## 修改脚本

可以参考/beaver/url_guide.lua 中的实现，这里添加了代码标注

```
require("class")
local ChttpHtml = require("httpHtml")

local CurlGuide = class("CurlIndex", ChttpHtml)

function CurlGuide:_init_(frame)
    ChttpHtml._init_(self)
    self._urlCb["/guide"] = function(tReq) return self:guide(tReq)  end     -- 声明链接与回调对应关系
    self._urlCb["/guide/hotplugin"] = function(tReq) return self:hotplugin(tReq)  end
    self._urlCb["/guide/oop"] = function(tReq) return self:oop(tReq)  end
    self._urlCb["/guide/pystring"] = function(tReq) return self:pystring(tReq)  end
    self._urlCb["/guide/webdevel"] = function(tReq) return self:webdevel(tReq)  end
    self:_install(frame)    --注册回调
end

local function loadFile(fPpath)   -- 加载文件
    local path = "../beaver/guide/" .. fPpath
    print(path)
    local f = io.open(path,"r")
    local s = f:read("*all")
    f:close()
    return s
end

function CurlGuide:guide(tReq)
    return {title="guide", content=self:markdown(loadFile("guide.md"))}   -- 采用markdown 引擎渲染文件
end

function CurlGuide:hotplugin(tReq)
    return {title="hotplugin", content=self:markdown(loadFile("hotplugin.md"))}
end

function CurlGuide:oop(tReq)
    return {title="oop", content=self:markdown(loadFile("oop.md"))}
end

function CurlGuide:pystring(tReq)
    return {title="pystring", content=self:markdown(loadFile("pystring.md"))}
end

function CurlGuide:webdevel(tReq)
    return {title="webdevel", content=self:markdown(loadFile("webdevel.md"))}
end

return CurlGuide
```

这里采用了面向对象方法实现，关于面向对象，可以[参考这里](/guide/oop)

## 热更新

* 如果仅修改了markdown文件，直接更新文件刷新页面即可；
* 如果修改了lua文件，给主进程发送1号信号，进程会重新装载，新页面也会立即生效；

[返回目录](/guide)