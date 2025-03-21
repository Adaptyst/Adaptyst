FROM gitlab-registry.cern.ch/adaptyst/gentoo-fp:latest
RUN mkdir -p /root/adaptyst
COPY . /root/adaptyst/
RUN emerge -g --quiet-build=y llvm-core/clang && emerge dev-build/ninja && cd /root/adaptyst && ./build.sh -G Ninja && { echo | PATH=$PATH:$(dirname /usr/lib/llvm/*/bin/clang) ./install.sh; } && cd .. && rm -rf adaptyst && emerge --deselect llvm-core/clang dev-build/ninja && emerge --depclean
RUN setcap cap_perfmon,cap_bpf,cap_ipc_lock,cap_syslog+ep /opt/adaptyst/perf/bin/perf && useradd -m gentoo-adaptyst
USER gentoo-adaptyst
