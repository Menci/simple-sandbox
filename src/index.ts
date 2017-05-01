import { SandboxParameter } from './interfaces';
import nativeAddon from './nativeAddon';
import { SandboxProcess } from './sandboxProcess';
const startSandbox = function (parameter: SandboxParameter) {
    return new Promise((res, rej) => {
        nativeAddon.StartChild(parameter, function (err, result) {
            if (err)
                rej(err);
            else
                res(new SandboxProcess(result.pid, parameter));
        });
    });
};

export { startSandbox };