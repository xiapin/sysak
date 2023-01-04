# 字符串处理

同为脚本语言，lua 默认的字符串处理并不像python 那么完善。但只要通过拓展，也可以像python 一样对字符串进行处理。当前已经实现了split/strip 等高频使用函数。参考[Python字符串处理](https://www.jianshu.com/p/b758332c44bb) 

头文件 `local pystring = require("pystring")`

## split

split 是 python 常用的字符串分割函数，它还有rsplit一个变体，用于字符串分割，提升字符串处理效率

***

```
-- 字符串分割，默认按照空格分割
local ret = pystring:split("hello lua language")
assert(#ret == 3)
assert(ret[1] == "hello")
assert(ret[2] == "lua")
assert(ret[3] == "language")

-- 自定符号分割
ret = pystring:split("hello*lua *language", "*")
assert(#ret == 3)
assert(ret[1] == "hello")
assert(ret[2] == "lua ")
assert(ret[3] == "language")

-- 从右边开始规定次数分割
ret = pystring:rsplit("hello*lua *language", "*", 1)
assert(#ret == 2)
assert(ret[1] == "hello*lua ")

-- 多字符串分割
ret = pystring:split("hello*lua *language", "*l")
assert(#ret == 3)
assert(ret[1] == "hello")
assert(ret[2] == "ua ")
assert(ret[3] == "anguage")
```


## strip

strip 用于剔除字符串两边不需要的字符，还有rstrip/lstrip两个实现，只删除左右两边不需要的字符数据。

***
```
-- strip掉左右空格
assert(pystring:strip("\t hello world.  \t\n") == "hello world.")

-- strip掉左右指定符号
assert(pystring:strip("**hello world**", "*") == "hello world")

-- strip复合符号
assert(pystring:strip("*?hello world*?", "*?") == "hello world")

-- strip字符串
assert(pystring:strip("abcdefhello worldabcdef", "abcdef") == "hello world")

-- lstrip字符串
assert(pystring:lstrip("abcdefhello worldabcdef", "abcdef") == "hello worldabcdef")

-- rstrip字符串
assert(pystring:rstrip("abcdefhello worldabcdef", "abcdef") == "abcdefhello world")
```

## join

join 是 split的逆运算，将字符串数组按照delim连接起来，形成一个新的字符串。   
不论是脚本还是编译语言，都应避免频繁申请新内存。针对需要组大字符串的场景，应该尽可能采用[join方式](https://blog.csdn.net/weixin_46491071/article/details/109786998)来组合大长字符串。  

***
```
-- join 连接
local s = "abc d ef g"
local ret = pystring:split(s)
assert(pystring:join(" ", ret) == s)
```

## startswith/endswith

startswith 用于判断字符串是否以指定字符串开头，endswith用于判断结尾 

***
```
-- startswith/endswith
assert(pystring:startswith("hello world", "hello"))
assert(pystring:endswith("hello world", "world"))
```

## find

find 用于子串查找，成功返回首次开始的位置，如果不包含，返回nil

***
```
-- find
assert(pystring:find("hello world.", "hello") == 1)
```

[返回目录](/guide)