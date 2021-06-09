gamescope can take advantage of the Linux ftrace infrastructure for event tracing.

1. Start the tracing script below
2. Start gamescope
3. Stop the script
4. Use [gpuvis](https://github.com/mikesart/gpuvis) to visualize the resulting trace

```sh
#!/bin/sh -eu

TRACEFS="/sys/kernel/tracing"
if ! [ -w "$TRACEFS/trace_marker" ]; then
	echo "Allowing unprivileged users to write to $TRACEFS/trace_marker"
	sudo chmod 0755 "$TRACEFS"
	sudo chmod 0222 "$TRACEFS/trace_marker"
	echo "Now restart the traced process and run this script again"
	exit 1
fi

trace-cmd record -i \
	-e "sched:sched_switch" \
	-e "sched:sched_process_exec" \
	-e "sched:sched_process_fork" \
	-e "sched:sched_process_exit" \
	-e "amdgpu:amdgpu_vm_flush" \
	-e "amdgpu:amdgpu_cs_ioctl" \
	-e "amdgpu:amdgpu_sched_run_job" \
	-e "*fence:*fence_signaled" \
	-e "drm:drm_vblank_event" \
	-e "drm:drm_vblank_event_queued" \
	-e "i915:i915_flip_request" \
	-e "i915:i915_flip_complete" \
	-e "i915:intel_gpu_freq_change" \
	-e "i915:i915_gem_request_add" \
	-e "i915:i915_gem_request_submit" \
	-e "i915:i915_gem_request_in" \
	-e "i915:i915_gem_request_out" \
	-e "i915:intel_engine_notify" \
	-e "i915:i915_gem_request_wait_begin" \
	-e "i915:i915_gem_request_wait_end"
```