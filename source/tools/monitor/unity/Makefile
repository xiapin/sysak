DEPMOD=beeQ

$(DEPMOD):ECHO
	make -C $@

clean:
	$(foreach N, $(DEPMOD),make clean -C $(N);)

ECHO:
	@echo $(DEPMOD)