package cmd

import (
	"github.com/chentao-kernel/cloud_ebpf/nginx"
	"github.com/chentao-kernel/cloud_ebpf/profile/command"
	"github.com/chentao-kernel/cloud_ebpf/profile/config"
	"github.com/spf13/cobra"
)

type Cmd struct {
	cfg     *config.Config
	rootCmd *cobra.Command
}

func NewCmd() *Cmd {
	var cfg config.Config
	rootCmd := command.NewRootCmd(&cfg)
	rootCmd.SilenceErrors = true
	return &Cmd{
		cfg:     &cfg,
		rootCmd: rootCmd,
	}
}

func (cmd *Cmd) Execute() {
	cmd.rootCmd.Execute()
}

func (cmd *Cmd) ProfileCmd() {
	command.ProfileCmdInit(cmd.cfg, cmd.rootCmd)
}

func (cmd *Cmd) NginxCmd() {
	nginx.NginxCmdInit(cmd.cfg, cmd.rootCmd)
}

func (cmd *Cmd) NetTraceCmd() {

}
