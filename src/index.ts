import { SandboxParameter } from './interfaces';
import nativeAddon from './nativeAddon';
import { SandboxProcess } from './sandboxProcess';
import { existsSync } from 'fs';
import * as randomString from 'randomstring';
import * as path from 'path';

if (!existsSync('/sys/fs/cgroup/memory/memory.memsw.usage_in_bytes')) {
    throw new Error("Your linux kernel doesn't support memory-swap account. Please turn it on following the readme.");
}

export async function startSandbox(parameter: SandboxParameter): Promise<SandboxProcess> {
    return new Promise<SandboxProcess>((res, rej) => {
        const actualParameter = Object.assign({}, parameter);
        actualParameter.cgroup = path.join(actualParameter.cgroup, randomString.generate(9));
        nativeAddon.StartChild(actualParameter, function (err, result) {
            if (err)
                rej(err);
            else
                res(new SandboxProcess(result.pid, actualParameter));
        });
    });
};
