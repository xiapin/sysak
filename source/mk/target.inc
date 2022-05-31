target_rule:
	pt=`pwd`;pt_prev=$${pt#*'source/tools/'};target_class=$${pt_prev%'/'*};\
	if [ -f README.md ]; then helpinfo=`grep 功能说明 README.md -A 1 | grep -v 功能说明`;fi;\
	echo $$target_class:$(target):$(DEPEND):help{$$helpinfo} >> $(TARGET_PATH)/$(SYSAK_RULES)
