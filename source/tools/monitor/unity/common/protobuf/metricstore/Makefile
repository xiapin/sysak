SO := libmetricSnappy.so

all: $(SO) install

$(SO):
	$(shell ./build_shared.sh)

install: $(SO)
	cp $(SO) ../../beeQ/lib

clean:
	rm -f $(SO)
