import requests
import yaml
import os

BASE_DIR = os.path.dirname(os.path.abspath(__file__))


def yaml_config():
    """load response code status"""
    yaml_name = "config.yaml"
    yaml_path = os.path.join(BASE_DIR, yaml_name)
    with open(yaml_path, 'r') as f:
        config = yaml.load(f, Loader=yaml.FullLoader)
    return config


config = yaml_config()
host_config = config["host_config"]


def get_request(url, params=None):
    try:
        response = requests.get(url, params=params)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.RequestException as e:
        print(e)


if __name__ == "__main__":
    get_request(url = host_config)
