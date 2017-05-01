var addon = require('bindings')('sandbox');

const ptsnum = 6;

addon.StartChild(
    {
        chroot: "/home/t123yh/alpine",
        binary: "/home/t123yh/bintest",
        working: "/home/t123yh/asdf",
        executable: "/bin/sh",
        parameters: ["/bin/sh"],
        environments: ["PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"],
        stdin: "/dev/pts/" + ptsnum,
        stdout: "/dev/pts/" + ptsnum,
        stderr: "/dev/pts/" + ptsnum,
        time: 1000,
        mountProc: true,
        redirectBeforeChroot: true,
        memory: 1024000 * 1024, // 100MB
        process: 10,
        user: "test",
        cgroup: "asdf"
    },
    function (err, result) {
        if (err)
            console.log("error: " + err);
        else {
            console.log("OK!!!" + JSON.stringify(result));
            process.exit();
        }
    }
);
