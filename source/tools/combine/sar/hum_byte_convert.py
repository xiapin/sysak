# -*- coding: utf-8 -*-
#!/root/anaconda3/envs/python310

def hum_convert(value):
    units = ["K", "M", "G", "TB", "PB"]
    size = 1024.0
    for i in range(len(units)):
        if (value / size) < 1:
            return "%.1f%s" % (value, units[i])
        value = value / size

def hum_byte_convert(value):
    units = ["B","K", "M", "G", "TB", "PB"]
    size = 1024.0
    max_int = 1000.0
    for i in range(len(units)):
        if (value / max_int) < 1:
            return "%.1f%s" % (value, units[i])
        value = value / size