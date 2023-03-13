package command

import (
	"fmt"
	"runtime"
	"strings"
	"text/tabwriter"

	"github.com/fatih/color"
	"github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/pflag"

	"github.com/chentao-kernel/cloud_ebpf/profile/config"
	"github.com/pyroscope-io/pyroscope/pkg/cli"
)

var (
	headerClr *color.Color
	itemClr   *color.Color
	descClr   *color.Color
	defClr    *color.Color
)

func ProfileCmdInit(cfg *config.Config, rootCmd *cobra.Command) {
	subcommands := []*cobra.Command{
		newONCPUSpyCmd(&cfg.ONCPU),
		newOFFCPUSpyCmd(&cfg.OFFCPU),
		newUSERSLOWSpyCmd(&cfg.USERSLOW),
		newPINGSLOWSpyCmd(&cfg.PINGSLOW),
		newVersionCmd(),
	}

	for _, c := range subcommands {
		if c == nil {
			continue
		}
		addHelpSubcommand(c)
		c.HasHelpSubCommands()
		rootCmd.AddCommand(c)
	}

	logrus.SetReportCaller(true)
	logrus.SetFormatter(&logrus.TextFormatter{
		TimestampFormat: "2006-01-02T15:04:05.000000",
		FullTimestamp:   true,
		CallerPrettyfier: func(f *runtime.Frame) (string, string) {
			filename := f.File
			if len(filename) > 38 {
				filename = filename[38:]
			}
			return "", fmt.Sprintf(" %s:%d", filename, f.Line)
		},
	})
}

func Execute() error {
	// 通过viper库将命令行的入参转成config结构体的成员
	var cfg config.Config
	rootCmd := NewRootCmd(&cfg)
	rootCmd.SilenceErrors = true

	subcommands := []*cobra.Command{
		newONCPUSpyCmd(&cfg.ONCPU),
		newOFFCPUSpyCmd(&cfg.OFFCPU),
		newUSERSLOWSpyCmd(&cfg.USERSLOW),
		newPINGSLOWSpyCmd(&cfg.PINGSLOW),
		newVersionCmd(),
	}

	for _, c := range subcommands {
		if c == nil {
			continue
		}
		addHelpSubcommand(c)
		c.HasHelpSubCommands()
		rootCmd.AddCommand(c)
	}

	logrus.SetReportCaller(true)
	logrus.SetFormatter(&logrus.TextFormatter{
		TimestampFormat: "2006-01-02T15:04:05.000000",
		FullTimestamp:   true,
		CallerPrettyfier: func(f *runtime.Frame) (string, string) {
			filename := f.File
			if len(filename) > 38 {
				filename = filename[38:]
			}
			return "", fmt.Sprintf(" %s:%d", filename, f.Line)
		},
	})

	return rootCmd.Execute()
}

func NewRootCmd(cfg *config.Config) *cobra.Command {
	vpr := newViper()
	rootCmd := &cobra.Command{
		Use: "raptor [flags] <subcommand>",
		Run: func(cmd *cobra.Command, _ []string) {
			if cfg.Version {
				printVersion(cmd)
			} else {
				printHelpMessage(cmd, nil)
			}
		},
	}

	rootCmd.SetUsageFunc(printUsageMessage)
	rootCmd.SetHelpFunc(printHelpMessage)
	cli.PopulateFlagSet(cfg, rootCmd.Flags(), vpr)
	return rootCmd
}

func printUsageMessage(cmd *cobra.Command) error {
	printHelpMessage(cmd, nil)
	return nil
}

func printHelpMessage(cmd *cobra.Command, _ []string) {
	cmd.Println(DefaultUsageFunc(cmd.Flags(), cmd))
}

func addHelpSubcommand(cmd *cobra.Command) {
	cmd.AddCommand(&cobra.Command{
		Use: "help",
		Run: func(_ *cobra.Command, _ []string) {
			printHelpMessage(cmd, nil)
		},
	})
}

func DefaultUsageFunc(sf *pflag.FlagSet, c *cobra.Command) string {
	var b strings.Builder

	if hasSubCommands(c) {
		headerClr.Fprintf(&b, "SUBCOMMANDS\n")
		tw := tabwriter.NewWriter(&b, 0, 2, 2, ' ', 0)
		for _, subcommand := range c.Commands() {
			if !subcommand.Hidden {
				fmt.Fprintf(tw, "  %s\t%s\n", itemClr.Sprintf(subcommand.Name()), subcommand.Short)
			}
		}
		tw.Flush()
		fmt.Fprintf(&b, "\n")
	}

	if countFlags(c.Flags()) > 0 {
		// headerClr.Fprintf(&b, "FLAGS\n")
		tw := tabwriter.NewWriter(&b, 0, 2, 2, ' ', 0)
		fmt.Fprintf(tw, "%s\t  %s@new-line@\n", headerClr.Sprintf("FLAGS"), defClr.Sprint("DEFAULT VALUES"))

		// TODO: it would be nice to sort by how often people would use these.
		//   But for that we'd have to have a conversion from flag-set back to struct
		sf.VisitAll(func(f *pflag.Flag) {
			if f.Hidden {
				return
			}
			def := f.DefValue
			// if def == "" {
			// 	def = "..."
			// }
			def = defClr.Sprint(def)
			// def = fmt.Sprintf("(%s)", def)
			fmt.Fprintf(tw, "  %s\t%s", itemClr.Sprintf("--"+f.Name), def)
			if f.Usage != "" {
				fmt.Fprintf(tw, "@new-line@    ")
				descClr.Fprint(tw, f.Usage)
			}
			descClr.Fprint(tw, "@new-line@")
			fmt.Fprint(tw, "\n")
		})
		tw.Flush()
		// fmt.Fprintf(&b, "\n")
	}

	if hasSubCommands(c) {
		b.WriteString("Run 'Raptor SUBCOMMAND --help' for more information on a subcommand.\n")
	}

	return strings.ReplaceAll(b.String(), "@new-line@", "\n")
}

func hasSubCommands(cmd *cobra.Command) bool {
	return cmd.HasSubCommands() && !(len(cmd.Commands()) == 1 && cmd.Commands()[0].Name() == "help")
}

func countFlags(fs *pflag.FlagSet) (n int) {
	fs.VisitAll(func(*pflag.Flag) { n++ })
	return n
}

func init() {
	headerClr = color.New(color.FgGreen)
	itemClr = color.New(color.Bold)
	descClr = color.New()
	defClr = color.New(color.FgYellow)
}
