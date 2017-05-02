# simple-sandbox
A simple linux sandbox with Node.js API.

## Prerequisites
### Packages
You need to have the build-essentials (g++, make, etc.) and the boost library (particularly, boost-system and boost-filesystem) installed in your system in order to build the C++ part.

Install them by (in Ubuntu):
```bash
apt install build-essential libboost-all-dev
```
### Kernel
You need to have the memory swap account (disabled by default in Debian 8) enabled with your kernel. You can verify this by checking the existence of `/sys/fs/cgroup/memory/memory.memsw.usage_in_bytes` 

If that file does not exist, then you may have to turn on that with your grub.

Add `swapaccount=1` to `GRUB_CMDLINE_LINUX_DEFAULT` section in `/etc/default/grub`.
```bash
GRUB_CMDLINE_LINUX_DEFAULT="quiet splash cgroup_enable=memory swapaccount=1"
```

And then run
```bash
update-grub && reboot
```
To enjoy the modified kernel.

## Build
Pull the repository to somewhere on your computer and run
```bash
yarn install # Install required packages and compile C++ code.
yarn run build # Compile typescript code.
```
To build the project. If you don't want to use `yarn`, just change `yarn` to `npm`.

You can use `yarn run build:watch` to watch for the change of typescript file.

## Use
The library is with a simple API.
To start the sandbox, use the following code:
```js
const sandbox = require('simple-sandbox');
const myProcess = await sandbox.startSandbox(parameters);
```
where `parameters` is an instance of the [SandboxParameters interface](src/interfaces.ts). The detail explanation is available in the comments in that file.

The startSandbox function returns a Promise, from which you can get an instance of the [sandboxProcess](src/sandboxProcess.ts) class, reprensenting your sandboxed Process.

To terminate the sandboxed process, just use the `stop()` function:
```js
myProcess.stop();
```

To wait for the sandboxed process to end, use the `waitForStop()` function, which returns a `Promise`:
```js
myProcess.waitForStop().then((result) => {
    console.log("OK! " + JSON.stringify(result));
}).catch((err) => {
    console.log("Whooops!" + err.toString());
});
```

Note that `myProcess` itself is a EventEmitter, so you can register `exit` (indicates that the child process exited), and `error` (indicates that some error happens) event listener on it.

### Note
When a sandbox is started, a event listener for the `exit` event on the `process` object is registered. When Node.js is about to exit, it will kill the sandboxed process.

However, the `exit` event won't be called if there are `SIGTERM` or `SIGINT` (Ctrl-C) signals sent to the Node.js process. You should let the `SIGTERM` and `SIGINT` handler to call `process.exit()` on the main process:

```js
const terminationHandler = () => {
    process.exit(1);
};

process.on('SIGTERM', terminationHandler);
process.on('SIGINT', terminationHandler);
```

## Example
A demostration is available in the `demo` directory.
In order to get the demostration running for every one, we create the directory `/opt/sandbox-test`.

```bash
sudo mkdir /opt/sandbox-test
sudo chown $(whoami) /opt/sandbox-test
mkdir /opt/sandbox-test/rootfs /opt/sandbox-test/binary /opt/sandbox-test/working
curl -L https://nl.alpinelinux.org/alpine/v3.5/releases/x86_64/alpine-minirootfs-3.5.2-x86_64.tar.gz | tar -xzvf - -C /opt/sandbox-test/rootfs
```

**(to be continued)**