const sss = require('../lib'),
    rl = require('readline');

const doThings = async () => {
    try {
        const sandboxedProcess = await sss.startSandbox({
            chroot: "/home/t123yh/alpine",
            binary: "/home/t123yh/bintest",
            working: "/home/t123yh/asdf",
            executable: "/sandbox/working/a.out",
            parameters: ["/usr/bin/yes"],
            environments: ["PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"],
            stdin: "/dev/null",
            stdout: "/dev/stdout",
            stderr: "/dev/stderr",
            time: 1000,
            mountProc: true,
            redirectBeforeChroot: true,
            memory: 1024000 * 1024, // 100MB
            process: 10,
            user: "nobody",
            cgroup: "asdf"
        });

        console.log("Sandbox started, press enter to stop");
        var stdin = process.openStdin();
        stdin.addListener("data", function (d) {
            sandboxedProcess.stop();
        });

        const result = await new Promise((res, rej) => {
            sandboxedProcess.on('exit', (result) => {
                res(result);
            });
            sandboxedProcess.on('error', (err) => {
                rej(err);
            });
            sandboxedProcess.waitForStop();
        });
        console.log("Your sandbox finished!" + JSON.stringify(result));
    } catch (ex) {
        console.log("Whooops! " + ex.toString());
    }
    process.exit();
};
doThings();
