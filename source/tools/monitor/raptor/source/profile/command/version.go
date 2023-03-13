package command

import (
	"github.com/spf13/cobra"
	"fmt"
	"strings"
)

var (
	Version string
	CommitId string
	ReleaseTime string
	GoVersion string
	Auhtor 	string
)
// https://juejin.cn/post/7168842512298147854
const tmplt = `
VERSION INFO
	VERSION: 		%s
	COMMIT ID:		%s
	RELEASE TIME:		%s 	
	GO VERSION:		%s
	AUTHOR: 		%s
`
func newVersionCmd() *cobra.Command {
	return &cobra.Command{
		Use:   "version",
		Args:  cobra.NoArgs,
		Short: "Print raptor version details",
		Run: func(cmd *cobra.Command, _ []string) {
			printVersion(cmd)
		},
	}
}

func VersionInfo() string {
	return fmt.Sprintf(strings.TrimSpace(tmplt), Version, CommitId, ReleaseTime, GoVersion, Auhtor)
}

func printVersion(cmd *cobra.Command) {
	cmd.Println(VersionInfo())
	cmd.Println()
}
