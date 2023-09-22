# -*- coding: utf-8 -*-
#!/root/anaconda3/envs/python310
import yaml
import os

BASE_DIR = os.path.dirname(os.path.abspath(__file__))

def load_resp_second_dist():
    """load response code status"""
    yaml_name = "/etc/sysak/base.yaml"
    yaml_path = os.path.join(os.path.expanduser('~'), yaml_name)
    with open(yaml_path, 'r') as f:
        config = yaml.load(f, Loader=yaml.FullLoader)
    return config

def sar_config():
    """load response code status"""
    yaml_name = "config.yaml"
    yaml_path = os.path.join(BASE_DIR, yaml_name)
    with open(yaml_path, 'r') as f:
        config = yaml.load(f, Loader=yaml.FullLoader)
    return config