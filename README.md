What is SysAK

SysAK(System Analysis Kit) is a toolbox that contains valuable tools for Linux SRE,
such as problem diagnosing, events monitoring/tracing, and operating of system and service.
These tools come from everyday work experience and other popular internal tools from Alibaba,
like diagnose-tools, ossre, NX, etc.

It is distributed under the Mulan Permissive Software License，Version 2 - see the
accompanying LICENSE file for more details.
And keep the origin License for the lib dir -include kernel modules and libbpf, which is compatible 
with user-mode tools.



Quick start to use SysAK:
1) ./configure
2) make
3) make install
4) sysak list -a

More usage documentation of SysAK are included in doc/
