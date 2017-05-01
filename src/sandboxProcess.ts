import { SandboxParameter, SandboxResult, SandboxStatus } from './interfaces';
import sandboxAddon from './nativeAddon';
import * as utils from './utils';
import * as events from 'events';

export class SandboxProcess extends events.EventEmitter {
    public readonly pid: number;
    public readonly parameter: SandboxParameter;
    private readonly cancellationToken: NodeJS.Timer = null;
    private readonly stopCallback: () => void;

    private countedCpuTime: number = 0;
    private actualCpuTime: number = 0;
    private timeout: boolean = false;
    private cancelled: boolean = false;

    public running: boolean = true;

    constructor(pid: number, parameter: SandboxParameter) {
        super();

        this.pid = pid;
        this.parameter = parameter;

        const myFather = this;
        // Stop the sandboxed process on Node.js exit.
        this.stopCallback = () => {
            myFather.stop();
        }
        process.addListener('exit', this.stopCallback);

        if (this.parameter.time != -1) {
            // Check every 50ms.
            const checkInterval = Math.min(this.parameter.time / 10, 50);
            this.cancellationToken = setInterval(() => {
                const val: number = Number(sandboxAddon.GetCgroupProperty("cpuacct", myFather.parameter.cgroup, "cpuacct.usage"));
                myFather.countedCpuTime += Math.max(
                    val - myFather.actualCpuTime,            // The real time, or if less than 40%,
                    utils.milliToNano(checkInterval) * 0.4 // 40% of actually elapsed time
                );
                myFather.actualCpuTime = val;

                // Time limit exceeded
                if (myFather.countedCpuTime > utils.milliToNano(parameter.time)) {
                    myFather.timeout = true;
                    myFather.stop();
                }
            }, checkInterval);
        }

        sandboxAddon.WaitForProcess(this.pid, (err, runResult) => {
            if (err) {
                myFather.stop();
                myFather.emit('error', err);
            } else {
                myFather.cleanup();
                const memUsage: number = Number(sandboxAddon.GetCgroupProperty("memory", myFather.parameter.cgroup, "memory.memsw.max_usage_in_bytes"));

                let result: SandboxResult = {
                    status: SandboxStatus.Unknown,
                    time: myFather.actualCpuTime,
                    memory: memUsage,
                    code: runResult.code
                };

                if (myFather.timeout) {
                    result.status = SandboxStatus.TimeLimitExceeded;
                } else if (myFather.cancelled) {
                    result.status = SandboxStatus.Cancelled;
                } else if (myFather.parameter.memory != -1 && memUsage > myFather.parameter.memory) {
                    result.status = SandboxStatus.MemoryLimitExceeded;
                } else if (runResult.status == 'signaled') {
                    result.status = SandboxStatus.RuntimeError;
                } else if (runResult.status == 'exited') {
                    result.status = SandboxStatus.OK;
                }

                myFather.emit('exit', result);
            }
        });
    }

    private cleanup() {
        if (this.running) {
            if (this.cancellationToken) {
                clearInterval(this.cancellationToken);
            }
            process.removeListener('exit', this.stopCallback);
            this.running = false;
        }
    }

    stop() {
        this.cancelled = true;
        try {
            console.log("KILLING " + this.pid);
            process.kill(this.pid, "SIGKILL");
        } catch (err) { }
        this.cleanup();
    }

    waitForStop(callback) {
        this.on('exit', (status) => {
            callback(null, status);
        });
        this.on('error', (err) => {
            callback(err, null);
        });
    }
};
