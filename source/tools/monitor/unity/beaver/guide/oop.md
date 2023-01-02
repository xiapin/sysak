# 面向对象（OOP）概述

面向对象设计具有易维护、易扩展、质量高、效率高等优势，参考[这里的总结和设计原则](https://www.cnblogs.com/sun_moon_earth/archive/2008/07/21/1247512.html)，适合中大型项目使用。当然，软件质量是由开发者最终决定的，而不是采用什么开发方法。

当前unity监控大部分应用开发采用OOP模式设计。**OOP 并不是Lua自带的特性**，故需要在Lua table 基础上，进行面向对象扩展支持。代码实现参考 /common/class.lua。

当前lua 面向对象支持特性梳理：

1. 支持单继承；
2. 类建议采用单独lua文件进行分装；
3. 只支持私有（local成员和方法）和公开（self）两种成员访问模式；
4. 通过 \_init\_ 实现构造
5. 通过 \_del\_ 实现析构

# show code

在guide/oop 目录下有 面向对象的功能代码， 继承关系：Cbase->Cone->Ctwo, Cone->Cthree。

## Cbase

顾名思义，这是一个基础类

	require("class")    -- 包含头文件
	
	local Cbase = class("base")     -- 声明为类，第一个传参是类名字，可以自己取，第二入参为要继承的基类
	
	function Cbase:_init_(name)   --类构造函数
	    self.name = name
	end
	
	function Cbase:hello()  --类成员方法
	    return self.name
	end
	
	function Cbase:_del_()   --类析构函数，按照 _del_方法命名
	    print("base del..." .. self.name)
	end

	return Cbase  -- 返回类声明，这里不要忘记
		

## Cone

这是一个继承类的实现。**注意构造函数里面中的调用父类函数的方法**, 不能采用 self:_init_的方法来调用。

	require("class")
	local Cbase = require("base")  --引用base类
	
	Cone = class("one", Cbase)  --从Cbase类继承
	
	function Cone:_init_(name)
	    Cbase._init_(self, name)   --调用父类函数。
	end

	function Cone:say()
	    print("one say " .. self.name)
	end
	
	return Cone


## Ctwo

Ctwo 继承于Cone，这里重新实现并复用了父类的say方法。

	require("class")
	local Cone = require("one")
	
	CTwo = class("two", Cone)
	
	function CTwo:_init_(name)
	    Cone._init_(self, name)
	end
	
	function CTwo:say()
	    print("two say " .. self.name)
	    print("super")
	    Cone.say(self)
	end
	
	return CTwo
	
## 类实例化 tobj

采用new函数来实例化类，注意， new 并不是类的方法，故不能用 Cone:new 的方法来做初始化。

	package.path = package.path .. ";../../common/?.lua;"
	
	local Cone = require("one")
	local Ctwo = require("two")
	local CThree = require("three")
	
	local one = Cone.new("1one")
	local two = Ctwo.new("2two")
	local three = CThree.new("3three")
	
	assert(one:hello() == "1one")
	assert(two:hello() == "2two")
	assert(three:hello() == "3three")
	
	one:say()
	two:say()
	three:say()
	
	one = nil  --销毁基础类，不会对子类带来任何影响。
	two:say()

## 关于:和.的区别

.是访问表成员的方法，和C语言中访问结构体成员的方法一致。
:是一种[语法糖](https://baike.baidu.com/item/%E8%AF%AD%E6%B3%95%E7%B3%96/5247005)，它会将self（类似C++中的this指针，或者python的中self）作为一个隐藏参数参数进行传递，以下两个操作是等价的。

	function Cone:say()
	function Cone.say(self)

[返回目录](/guide)
