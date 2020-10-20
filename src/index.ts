import { SandboxParameter } from './interfaces';
import nativeAddon from './nativeAddon';
import { SandboxProcess } from './sandboxProcess';
import { existsSync } from 'fs';
import * as randomString from 'randomstring';
import * as path from 'path';

export * from './interfaces';

if (!existsSync('/sys/fs/cgroup/memory/memory.memsw.usage_in_bytes')) {
    throw new Error("Your linux kernel doesn't support memory-swap account. Please turn it on following the readme.");
}

export function startSandbox(parameter: SandboxParameter): SandboxProcess {
    const actualParameter = Object.assign({}, parameter);
    actualParameter.cgroup = path.join(actualParameter.cgroup, randomString.generate(9));
    const startResult: { pid: number; execParam: ArrayBuffer } = nativeAddon.startSandbox(actualParameter);
    return new SandboxProcess(actualParameter, startResult.pid, startResult.execParam);
};

