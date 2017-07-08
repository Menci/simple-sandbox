import { SandboxParameter, SandboxResult, SandboxStatus } from './interfaces';
import sandboxAddon from './nativeAddon';
import * as utils from './utils';
import * as events from 'events';
import * as util from 'util';

const removeCgroup = util.promisify(sandboxAddon.RemoveCgroup);

export class SandboxProcess {
    public readonly pid: number;
    public readonly parameter: SandboxParameter;
    private readonly cancellationToken: NodeJS.Timer = null;
    private readonly stopCallback: () => void;

    private countedCpuTime: number = 0;
    private actualCpuTime: number = 0;
    private timeout: boolean = false;
    private cancelled: boolean = false;
    private waitPromise: Promise<SandboxResult> = null;

    // A nasty hack
    private cleanupPromise: Promise<void>;
    private cleanupCallback: () => void;
    private cleanupErrCallback: (err: Error) => void;

    public running: boolean = true;

    constructor(pid: number, parameter: SandboxParameter) {
        this.pid = pid;
        this.parameter = parameter;

        const myFather = this;
        // Stop the sandboxed process on Node.js exit.
        this.stopCallback = () => {
            myFather.stop();
        }

        this.cleanupPromise = new Promise<void>((res, rej) => {
            this.cleanupCallback = res;
            this.cleanupErrCallback = rej;
        })

        let checkIfTimedOut = () => {};
        if (this.parameter.time !== -1) {
            // Check every 50ms.
            const checkInterval = Math.min(this.parameter.time / 10, 50);
            let lastCheck = new Date().getTime();
            checkIfTimedOut = () => {
                let current = new Date().getTime();
                const spent = current - lastCheck;
                lastCheck = current;
                const val: number = Number(sandboxAddon.GetCgroupProperty("cpuacct", myFather.parameter.cgroup, "cpuacct.usage"));
                myFather.countedCpuTime += Math.max(
                    val - myFather.actualCpuTime,            // The real time, or if less than 40%,
                    utils.milliToNano(spent) * 0.4 // 40% of actually elapsed time
                );
                myFather.actualCpuTime = val;

                // Time limit exceeded
                if (myFather.countedCpuTime > utils.milliToNano(parameter.time)) {
                    myFather.timeout = true;
                    myFather.stop();
                }
            };
            this.cancellationToken = setInterval(checkIfTimedOut, checkInterval);
        }

        this.waitPromise = new Promise((res, rej) => {
            sandboxAddon.WaitForProcess(this.pid, (err, runResult) => {
                if (err) {
                    myFather.stop();
                    myFather.cleanup();
                    rej(err);
                } else {
                    const memUsageWithCache: number = Number(sandboxAddon.GetCgroupProperty("memory", myFather.parameter.cgroup, "memory.memsw.max_usage_in_bytes"));
                    const cache: number = Number(sandboxAddon.GetCgroupProperty2("memory", myFather.parameter.cgroup, "memory.stat", "cache"));
                    const memUsage = memUsageWithCache - cache;

                    checkIfTimedOut();
                    myFather.cleanup();

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
                    } else if (runResult.status === 'signaled') {
                        result.status = SandboxStatus.RuntimeError;
                    } else if (runResult.status === 'exited') {
                        result.status = SandboxStatus.OK;
                    }

                    res(result);
                }
            });
        });
    }

    private removeCgroup(): void {
        Promise.all([removeCgroup("memory", this.parameter.cgroup),
        removeCgroup("cpuacct", this.parameter.cgroup),
        removeCgroup("pids", this.parameter.cgroup)])
            .then(() => { this.cleanupCallback(); }, (err) => { this.cleanupErrCallback(err); });
    }

    private cleanup(): void {
        if (this.running) {
            if (this.cancellationToken) {
                clearInterval(this.cancellationToken);
            }
            process.removeListener('exit', this.stopCallback);
            this.removeCgroup();
            this.running = false;
        }
    }

    stop(): void {
        this.cancelled = true;
        try {
            process.kill(this.pid, "SIGKILL");
        } catch (err) { }
    }

    async waitForStop(): Promise<SandboxResult> {
        return await this.waitPromise;
    }

    async waitForCleanedUp(): Promise<void> {
        await this.cleanupPromise;
    }
};
