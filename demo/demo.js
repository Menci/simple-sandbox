const sss = require('../lib'),
    rl = require('readline');

const terminationHandler = () => {
    process.exit(1);
};

process.on('SIGTERM', terminationHandler);
process.on('SIGINT', terminationHandler);

const doThings = async () => {
    try {
        const sandboxedProcess = await sss.startSandbox({
            chroot: "/opt/sandbox-test/rootfs",
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
            executable: "/usr/bin/yes",
            parameters: ["/usr/bin/yes"],
            environments: ["PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"],
            stdin: "/dev/null",
            stdout: "/dev/stdout",
            stderr: "/dev/stdout",
            time: 1000,
            mountProc: true,
            redirectBeforeChroot: true,
            memory: 10240 * 1024, // 10MB
            process: 10,
            user: "nobody",
            cgroup: "asdf",
            workingDirectory: "/sandbox/working"
        });

        console.log("Sandbox started, press enter to stop");
        var stdin = process.openStdin();
        stdin.addListener("data", function (d) {
            sandboxedProcess.stop();
        });

        const result = await sandboxedProcess.waitForStop();
        console.log("Your sandbox finished!" + JSON.stringify(result));
    } catch (ex) {
        console.log("Whooops! " + ex.toString());
    }
    process.exit();
};
doThings();
