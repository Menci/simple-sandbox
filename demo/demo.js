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
            binary: "/opt/sandbox-test/binary",
            working: "/opt/sandbox-test/working",
            executable: "/bin/sh",
            parameters: ["/bin/sh"],
            environments: ["PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"],
            stdin: "test.in",
            stdout: "test.out",
            stderr: "test.err",
            time: 10000000,
            mountProc: true,
            redirectBeforeChroot: false,
            memory: 10240 * 1024, // 10MB
            process: 10,
            user: "nobody",
            cgroup: "asdf"
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
