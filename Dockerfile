FROM gitlab-registry.cern.ch/adaptyst/gentoo-fp:latest
RUN mkdir -p /root/adaptyst
COPY . /root/adaptyst/
ARG JOBS=1
RUN MAKEOPTS="-j${JOBS}" USE="-extra" emerge --quiet-build=y sys-devel/clang && cd /root/adaptyst && ./build.sh && { echo | PATH=$PATH:$(dirname /usr/lib/llvm/*/bin/clang) ./install.sh; } && cd .. && rm -rf adaptyst && emerge --deselect sys-devel/clang sys-devel/llvm && emerge --depclean
RUN setcap cap_perfmon,cap_bpf+ep /opt/adaptyst/perf/bin/perf && useradd -m gentoo-adaptyst
USER gentoo-adaptyst
