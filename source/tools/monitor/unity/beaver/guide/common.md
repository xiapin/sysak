# common 通用库函数说明

lua 自身提供的库函数比较少，和python 相比使用起来还没有那么方便。在unity-mon 开发过程中，积累了一些通用的函数库操作，可以加速lua 开发过程。

## system 库

system 主要提供了一些lua 语言系统本身的一些相关操作（或许命名叫做lib库更为贴切，但已经成为习惯，现阶段还是继续沿用）。

### system:sleep(t)

* 函数说明：程序进入睡眠
* 传参 1：t， 单位 秒
* 返回值：无

### system:deepcopy(object)

* 函数说明：深度拷贝对象， 关于对象深度概念，可以参考 [python 说明](https://www.runoob.com/w3cnote/python-understanding-dict-copy-shallow-or-deep.html)
* 传参 1：object
* 返回值：深度拷贝出来的对象

### system:dump(t)

* 函数说明：将对象序列化成字符串。在lua 中 默认print table 是不会对table 内容进行展开的，可以使用dump 方法将 table 中的数据全面展开出来。
* 传参 1：t， 对象
* 返回值：序列化字符串的对象

### system:dumps(t)

* 函数说明：参考 system:dump，不一样的是dumps 会在dump的基础上直接调用print 打印出来
* 传参 1：t， 对象
* 返回值：无

### system:keyIsIn(tbl, key)

* 函数说明：在hash table 中， 判断key 是否在table 中
* 传参 1：tbl， table 名
* 传参 2： key， key 名
* 返回值：如果存在返回true，否则返回false

### system:valueIsIn(tbl, value)

* 函数说明：在hash table 或者 list table 中， 判断value 是否在table 中
* 传参 1：tbl， table 名
* 传参 2： value， value 名
* 返回值：如果存在返回true，否则返回false

### system:valueIndex(tbl, value)

* 函数说明：在list table 中， 获取value 的index
* 传参 1：tbl， table 名
* 传参 2： value， value 名
* 返回值：如果存在返回index， 否则返回0

### system:keyCount(tbl)

* 函数说明：在hash table 中， 获取tbl 中 key 数量。与 #tbl 操作不一样的是， table 可能是hash/list 的混合提，如果只希望获取tbl 中的 hash key 数量，可使用 keyCount 进行获取
* 传参 1：tbl， table 名
* 返回值：hash key 数量

### system:dictCopy(tbl)

* 函数说明：浅拷贝 tbl，关于table 深拷贝和浅拷贝的差别，可以自行搜索一下
* 传参 1：tbl， table 名
* 返回值：新key的位置


### system:listMerge(...)

* 函数说明：合并 list table。
* 传参: 变参，list 列表
* 返回值：新的list table。

### system:hex2ups(hex)

* 函数说明：将二进制流转换成大写的HEX字符串。
* 传参 1：hex， 要dump的二进制流
* 返回值：HEX 后的字符串

### system:hex2lows(hex)

* 函数说明：将二进制流转换成小写的hex字符串。
* 传参 1：hex， 要dump的二进制流
* 返回值：hex 后的字符串

### system:hexdump(buf)

* 函数说明：以hexdump的方式输出流数据，效果参考hexdump 命令
* 传参 1：buf， 要dump的二进制流
* 返回值：hexdump 后的字符串

### system:escHtml(s)

* 函数说明：转义字符串，对于html 字符会做转义处理
* 传参 1：s， 要转义的字符串
* 返回值：转义后的字符串

### system:escMd(s)

* 函数说明：转义字符串，对于markdown 字符会做转义处理
* 传参 1：s， 要转义的字符串
* 返回值：转义后的字符串

### system:timeRfc1123(t)

* 函数说明：对unix 时间戳以 rfc1123 的格式进行输出
* 传参 1：t，unix 时间戳
* 返回值：时间戳字符串

### system:parseYaml(fYaml)

* 函数说明：解析yaml 文件
* 传参 1：fYaml 文件目录
* 返回值：yaml 对应的table 对象，如果yaml 解析异常或者文件不存在，将会直接报error

### system:posixError(msg, err, errno)

* 函数说明：上报系统调用异常，经常与posix 里面的库函数配套使用
* 传参 1：msg 自定义输出的消息
* 传参 2：err 标准 error 错误输出信息
* 传参 3：errno 错误码
* 返回值：该函数不会返回，会直接抛出异常

## pystring

python 最擅长在于字符串处理。可以参考这里的[官方库说明](https://docs.python.org/zh-cn/3/library/string.html)，pystring 库通过lua 实现了绝大部分python 常用的string 处理函数。让lua 处理字符串可以像python 一样便捷易于上手。

使用 pystring 库是要注意以下事项：

1. 当前unity\-mon 采用的是lua 自带的正则匹配方法，未集成regexp 标准正则库；
2. pystring 中提供的绝大部分函数都与python的处理方法保持一致，但是不保证百分百兼容，故使用时建议根据实际用例测试一遍，可以参考[测试用例函数](https://gitee.com/anolis/sysak/blob/opensource_branch/source/tools/monitor/unity/test/string/py.lua)

### pystring:shift(s, n)

* 函数说明：字符串移位操作，参考python 的 << 和 >> 操作
* 传参 1：s 要移位的目标字符串
* 传参 2：n 要移位位数和方向，正数代表右移，负数表示左移
* 返回值：移位以后的字符串

### pystring:islower(s)

* 函数说明：判断字符串是否全部为小写字母
* 参数1： s 目标字符串
* 返回值：true/false

### pystring:isupper(s)

* 函数说明：同python 实现
* 参数1： s 目标字符串
* 返回值：true/false

### pystring:ishex(s)

* 函数说明：判断目标字符串是否为hex 字符串，含0-9,a-f,A-F
* 参数1： s 目标字符串
* 返回值：true/false

### pystring:isalnum(s)

* 函数说明：同python 实现
* 参数1： s 目标字符串
* 返回值：true/false


### pystring:istilte(s)

* 函数说明：判断单词是否符合首字母大写，其余字母小写的方式
* 参数1： s 目标字符串
* 返回值：true/false

### pystring:isfloat(s)

* 函数说明：判断字符串是否为浮点数呈现形式
* 参数1： s 目标字符串
* 返回值：true/false

### pystring:lower(s)

* 函数说明：同python 实现，转小写
* 参数1： s 目标字符串
* 返回值：处理后的字符串

### pystring:upper(s)

* 函数说明：同python 实现，转大写
* 参数1： s 目标字符串
* 返回值：处理后的字符串

### pystring:swapcase(s)

* 函数说明：同python 实现，交换大小写
* 参数1： s 目标字符串
* 返回值：处理后的字符串

### pystring:capitalize(s)

* 函数说明：单词首字母大写
* 参数1： s 目标字符串
* 返回值：处理后的字符串

### pystring:title(s)

* 函数说明：字符串中所有单词首字母大写
* 参数1： s 目标字符串
* 返回值：处理后的字符串

### pystring:ljust(s, len, ch)

* 函数说明：将字符串s 按照 len 长度左对齐，对齐部分填 ch
* 参数1： s 目标字符串
* 参数2：len 对齐长度
* 参数3：ch 填充字符，默认为空格，必须是单字符，否则会抛异常
* 返回值：对齐后的字符串


### pystring:rjust(s, len, ch)

* 函数说明：将字符串s 按照 len 长度右对齐，对齐部分填 ch
* 参数1： s 目标字符串
* 参数2：len 对齐长度
* 参数3：ch 填充字符，默认为空格，必须是单字符，否则会抛异常
* 返回值：对齐后的字符串

### pystring:center(s, len, ch)

* 函数说明：将字符串s 按照 len 长度中间对齐，对齐部分填 ch
* 参数1： s 目标字符串
* 参数2：len 对齐长度
* 参数3：ch 填充字符，默认为空格，必须是单字符，否则会抛异常
* 返回值：对齐后的字符串

### pystring:zfill(s, len)

* 函数说明：将字符串s 按照 len 长度向左填0
* 参数1： s 目标字符串
* 参数2：len 对齐长度
* 返回值：填齐后的字符串

### pystring:split(s, delimiter, n)

* 函数说明：将字符串s 按照 delimiter 分隔符进行分割
* 参数1： s 目标字符串
* 参数2：delimiter 分隔符，如果为空，则按照 多空格模式进行分割
* 参数3：n 分割次数，默认为最多次数
* 返回值：分割后的字符串数组

### pystring:split(s, delimiter, n)

* 函数说明：将字符串s 按照 delimiter 分隔符进行分割
* 参数1： s 目标字符串
* 参数2：delimiter 分隔符，如果为空，则按照 多空格模式进行分割
* 参数3：n 分割次数，默认为最多次数
* 返回值：分割后的字符串数组

