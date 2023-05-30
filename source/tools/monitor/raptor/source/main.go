/*
 *  Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
package main

import (
	"github.com/chentao-kernel/cloud_ebpf/cmd"
	"github.com/chentao-kernel/cloud_ebpf/profile/command"

	//"github.com/chentao-kernel/cloud_ebpf/profile/agent"
	_ "github.com/chentao-kernel/cloud_ebpf/nginx"
)

var (
	version     string
	commitId    string
	releaseTime string
	goVersion   string
	author      string
)

func main() {
	c := cmd.NewCmd()
	c.ProfileCmd()
	c.NginxCmd()
	c.Execute()
}

func init() {
	command.Version = version
	command.CommitId = commitId
	command.ReleaseTime = releaseTime
	command.GoVersion = goVersion
	command.Auhtor = author
}
