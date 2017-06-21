import { SandboxParameter } from './interfaces';
import nativeAddon from './nativeAddon';
import { SandboxProcess } from './sandboxProcess';
import { existsSync } from 'fs';

if (!existsSync('/sys/fs/cgroup/memory/memory.memsw.usage_in_bytes')) {
    throw new Error("Your linux kernel doesn't support memory-swap account. Please turn it on following the readme.");
}

export async function startSandbox(parameter: SandboxParameter): Promise<SandboxProcess> {
    return new Promise<SandboxProcess>((res, rej) => {
        nativeAddon.StartChild(parameter, function (err, result) {
            if (err)
                rej(err);
            else
                res(new SandboxProcess(result.pid, parameter));
        });
    });
};
