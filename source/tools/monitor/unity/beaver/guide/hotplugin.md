# 插件化

unity监控采用[yaml](http://yaml.org/)对插件进行管理，当前插件分为采集和输出两个部分进行管理：

## 采集侧（collector）

在collector/plugin.yaml 文件中，示例文件：

	plugins:
  	-  # 数组成员标志
	    so: sample     # so名，对应 collector/native 目录下应当存在 libsample.so
	    description: "just a example."   # 插件描述，当前监控系统未使用
	  
## 采集侧示例代码

在collector/plugin/sample 目录下有一个示例工程，它的本质其实就是一个so文件的编译项目。首先要看下sample 同级目录下的公共头文件 plugin_head.h，该头文件提供了数据生成的API，降低开发者实现难度。


	/// \brief 申请数据行数量，在填入数据前统一申请，根据实际情况填入
	/// \param lines 数据结构体
	/// \param num 申请行号数量
	/// \return 成功返回 0
	inline int unity_alloc_lines(struct unity_lines * lines, unsigned int num) __attribute__((always_inline));
	/// \brief 获取对应行数据，用于填入数据
	/// \param lines 数据结构体
	/// \param i 对应行下标
	/// \return 返回对应的数据行
	inline struct unity_line * unity_get_line(struct unity_lines * lines, unsigned int i) __attribute__((always_inline));
	/// \brief 设置数据行 表名
	/// \param line 行指针
	/// \param table 表名
	/// \return 成功返回 0
	inline int unity_set_table(struct unity_line * line, const char * table) __attribute__((always_inline));
	/// \brief 设置数据行 索引信息
	/// \param line 行指针
	/// \param i 索引下标
	/// \param name 索引名
	/// \param index 索引内容
	/// \return 成功返回 0
	inline int unity_set_index(struct unity_line * line, unsigned int i, const char * name, const char * index) __attribute__((always_inline));
	/// \brief 设置数据行 指标信息
	/// \param line 行指针
	/// \param i 指标下标
	/// \param name 指标名
	/// \param value 指标内容
	/// \return 成功返回 0
	inline int unity_set_value(struct unity_line * line, unsigned int i, const char * name, double value) __attribute__((always_inline));
	

### 数据行规格限制

1. unity\_set\_table 中 table 参数长度应该小于32（不含）
2. unity\_set\_index 中 name、index和unity\_set\_value 中 name 参数长度应该要小于16（不含）
3. unity\_set\_index 下标从0开始，并小于 4，即最多4个索引。而且下标数值应该连续，否则数据会从留白处截断
4.  unity\_set\_index 下标从0开始，并小于 32，即最多32个数值。而且下标数值应该连续，否则数据会从留白处截断


### sample 用例代码

参考 sample.c

	/// \brief 插件构造函数，在加载so的时候，会调用一次init
	/// \param arg 当前未使用，为NULL
	/// \return 成功返回 0
	int init(void * arg) {
	    printf("sample plugin install.\n");
	    return 0;
	}
	
	/// \brief 插件调用函数，通过调用在函数来收集要采集的指标
	/// \param t，间隔周期，如15s的采样周期，则该值为15
	/// \param lines 数值指针，用于填充采集到的数据。
	/// \return 成功返回 0
	int call(int t, struct unity_lines* lines) {
	    static double value = 0.0;
	    struct unity_line* line;
	
	    unity_alloc_lines(lines, 2);
	    line = unity_get_line(lines, 0);
	    unity_set_table(line, "sample_tbl1");
	    unity_set_index(line, 0, "mode", "sample1");
	    unity_set_value(line, 0, "value1", 1.0 + value);
	    unity_set_value(line, 1, "value2", 2.0 + value);
	
	    line = unity_get_line(lines, 1);
	    unity_set_table(line, "sample_tbl2");
	    unity_set_value(line, 0, "value1", 3.0 + value);
	    unity_set_value(line, 1, "value2", 4.0 + value);
	    unity_set_value(line, 2, "value3", 3.1 + value);
	    unity_set_value(line, 3, "value4", 4.1 + value);
	
	    value += 0.1;
	    return 0;
	}
	
	/// \brief 插件析构函数，调用完该函数时，必须要确保该插件已申请的资源已经全部释放完毕。
	/// \return 成功返回 0
	void deinit(void) {
	    printf("sample plugin uninstall\n");
	}

### 工程编译和安装

执行make完成编译，编译成功后，**执行make install** 将so复制到 collector/plugin/native目录下。

# 热更新

往unity 监控主进程发送一个1号（SIGHUP）信号，即可完成热更新。

# 输出侧更新

此时数据只是已经更新入库了，但是要在nodexport上面显示，需要配置beaver/export.yaml 文件，才能将查询从数据表中更新。

[返回目录](/guide)