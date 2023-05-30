package config

import (
	"io/ioutil"
	"gopkg.in/yaml.v2"
	"log"
)

var config *ConfYaml
type ConfYaml struct {
	Netobserv struct {
		Proto []string `yaml:"proto"`
		Pid int `yaml:"pid"`
		Sample int `yaml:"sample"`
		Port int `yaml:"port"`
	}
	Filepath struct {
		Btf string `yaml:"btf"`
		SoFile string `yaml:"sofile"`
	}
}

func ConfigInit() error {
	config = new(ConfYaml)
	file, err := ioutil.ReadFile("config.yaml")
	if err != nil {
		log.Printf("Read yaml config failed, err:%v.", err)
		return err
	}

	err = yaml.Unmarshal(file, config)
	if err != nil {
		log.Printf("Yaml unmarshel failed.")
		return err
	}
	log.Printf("%v\n", config)
	return nil
}

func ConfigGet() * ConfYaml {
	return config
}
