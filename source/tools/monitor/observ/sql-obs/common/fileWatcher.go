package common

import (
    "bufio"
    //"fmt"
    // "unsafe"
    "github.com/fsnotify/fsnotify"
    "os"
    // "strconv"
    "time"
    "sync"
)

type fileOpWatcherHandler func(event *fsnotify.Event, priData *interface{}) int
const (
    Handle_Done = 1
    Handle_Exited = 2
)

const (
    Has_data = 1
    Watcher_Exited = 2
)

type fileWriteWatcher struct {
    fileName string
    data []string
    changeLines int
    status chan int
}

func getFileTailPos(file *os.File) (int64, error) {
    readPos, err := file.Seek(0, os.SEEK_END)
    if err != nil {
        return -1, err
    }
    return readPos, nil
}

func isRegularFile(file *os.File) bool {
    fileInfo, _ := file.Stat()
    if fileInfo.Mode().IsRegular() {
        return true
    } else {
        return false
    }
}

func readLastLines(buf *[]string, scanner *bufio.Scanner,
    file *os.File, startPos *int64) (int, error) {
    lines := 0
    endPos, err := getFileTailPos(file)
    if err != nil {
        return -1, err
    }

    if endPos > 0 && endPos < *startPos {
        *startPos = 0
    }

    if *startPos < endPos {
        _, err = file.Seek(*startPos, 0)
        if err != nil {
            return -1, err
        }
    }

    for scanner.Scan() {
        line := scanner.Text()
        (*buf)[lines] = line
        *startPos += int64(len(line) + 1)
        lines += 1
        if lines >= cap(*buf) || (endPos > 0 && *startPos >= endPos) {
            break
        }
    }

    if err := scanner.Err(); err != nil {
        return -1, err
    }
    return lines, nil
}

func (fw *fileWriteWatcher)readNotRegularFile(scanner *bufio.Scanner,
    file *os.File, startPos *int64) {
    lines := 0
    var dataNotifyLock sync.Mutex
    lineChan := make(chan string)
    errChan := make(chan error)
    go func() {
        for scanner.Scan() {
            line := scanner.Text()
            lineChan <- line
        }
        errChan <- scanner.Err()
    }()
    timeout := 2 * time.Second
    timer := time.NewTimer(timeout)
    defer timer.Stop()
    for {
        select {
            case line := <-lineChan:
                timer.Reset(timeout)
                dataNotifyLock.Lock()
                fw.data[lines] = line
                lines += 1
                if lines >= cap(fw.data) {
                    fw.changeLines = lines
                    fw.status <- Has_data
                    lines = 0
                }
                dataNotifyLock.Unlock()
            case err := <-errChan:
                if err != nil {
                    return
                }
            case <-timer.C:
                dataNotifyLock.Lock()
                if lines > 0 {
                    fw.changeLines = lines
                    fw.status <- Has_data
                    lines = 0
                }
                dataNotifyLock.Unlock()
        }
    }
}

func (fw *fileWriteWatcher) readChangedFile() {
    file, err := os.Open(fw.fileName)
    if err != nil {
        PrintSysError(err)
        return
    }
    defer file.Close()

    startPos, err := getFileTailPos(file)
    if err != nil {
        PrintSysError(err)
        return
    }

    watcher, err := fsnotify.NewWatcher()
    if err != nil {
        PrintSysError(err)
        return
    }
    defer watcher.Close()

    err = watcher.Add(fw.fileName)
    if err != nil {
        PrintSysError(err)
        return
    }

    //Scanner for reading recent changes in files
    scanner := bufio.NewScanner(file)
    scanner.Split(bufio.ScanLines)

    //Default read up to 5000 lines at a time
    defaultCapacity := cap(fw.data)
    if defaultCapacity == 0 {
        defaultCapacity = 5000
    }
    if cap(fw.data) == 0 || len(fw.data) != cap(fw.data) {
        for i := len(fw.data); i < defaultCapacity; i++ {
            fw.data = append(fw.data, "")
        }
    }

    //Start watching...
    if isRegularFile(file) {
        for {
            select {
            case event := <-watcher.Events:
                //File write event occurs
                if event.Op&fsnotify.Write == fsnotify.Write {
                    lines, err := readLastLines(
                        &fw.data, scanner, file, &startPos)
                    if err != nil {
                        PrintSysError(err)
                        return
                    }
                    fw.changeLines = lines
                    fw.status <- Has_data
                }
            case err := <-watcher.Errors:
                PrintSysError(err)
                return
            }
        }
    } else {
        fw.readNotRegularFile(scanner, file, &startPos)
    }
}

func (fw *fileWriteWatcher) StartWatch() {
    //done := make(chan bool)
    go fw.readChangedFile()
    //<-done
}

func (fw *fileWriteWatcher) Data() ([]string) {
    return fw.data
}

func (fw *fileWriteWatcher) ChangeLines() (int) {
    return fw.changeLines
}

func (fw *fileWriteWatcher) Status() (chan int) {
    return fw.status
}

func StartFilesOpWatcher(files []string, Op fsnotify.Op,
    handle fileOpWatcherHandler, closeSource func(*interface{}),
    priData *interface{}) error {
    go func() {
        watcher, err := fsnotify.NewWatcher()
        if err != nil {
            PrintSysError(err)
            return
        }
        defer watcher.Close()
    
        for _, f := range files {
            err = watcher.Add(f)
            if err != nil {
                PrintSysError(err)
                return
            }
        }
        if closeSource != nil {
            defer closeSource(priData)
        }
        for {
            select {
            case event := <-watcher.Events:
                if event.Op&Op == Op {
                    ret := handle(&event, priData)
                    if ret != Handle_Done {
                        return
                    }
                }
            case err := <-watcher.Errors:
                PrintSysError(err)
                return
            }
        }
    }()
    return nil
}

/**
 * new_file_write_watcher - Create a monitor to monitor the specified file
 *                          and obtain the latest updated data
 * fileName: Target file for monitoring
 * readlines: After the file is updated, read the last 'readlines' lines
 *            if readlines is 0, Read all newly added data
 *
 * If the function is successful, a fileWriteWatcher object will return
 */
 func NewFileWriteWatcher(fileName string, readlines int) (fileWriteWatcher) {
    fw := fileWriteWatcher{
        fileName: fileName,
        data: make([]string, readlines),
        changeLines: 0,
        status: make(chan int),
    }
    return fw
}