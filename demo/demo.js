const sss = require('../lib'),
    rl = require('readline');

const terminationHandler = () => {
    process.exit(1);
};

process.on('SIGTERM', terminationHandler);
process.on('SIGINT', terminationHandler);

const doThings = async () => {
    try {
        const rootfs = "/opt/sandbox-test/rootfs"
        const sandboxedProcess = sss.startSandbox({
            hostname: "qwq",
            chroot: rootfs,
            mounts: [
                {
                    src: "/opt/sandbox-test/binary",
                    dst: "/sandbox/binary",
                    limit: 0
                }, {
                    src: "/opt/sandbox-test/working",
                    dst: "/sandbox/working",
                    limit: 10240 * 1024
                }],
            executable: "/bin/bash",
            parameters: ["/bin/bash"],
            environments: ["PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"],
            stdin: "/dev/stdin",
            stdout: "/dev/stdout",
            stderr: "/dev/stdout",
            time: 60 * 1000, // 1 minute, for a bash playground
            mountProc: true,
            redirectBeforeChroot: true,
            memory: 102400 * 1024, // 100MB
            process: 30,
            user: sss.getUidAndGidInSandbox(rootfs, "sandbox"),
            cgroup: "asdf",
            workingDirectory: "/sandbox/working"
        });

        // Uncomment these and change 'stdin: "/dev/stdin"' to "/dev/null" to cancel the sandbox with enter
        //
        // console.log("Sandbox started, press enter to stop");
        // var stdin = process.openStdin();
        // stdin.addListener("data", function (d) {
        //     sandboxedProcess.stop();
        // });

        const result = await sandboxedProcess.waitForStop();
        console.log("Your sandbox finished!" + JSON.stringify(result));
    } catch (ex) {
        console.log("Whooops! " + ex.toString());
    }
    process.exit();
};
doThings();
