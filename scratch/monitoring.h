#ifndef MONITORING_H
#define MONITORING_H

#include "ns3/core-module.h"
#include <ns3/switch-node.h>
#include <cstdio>
#include <map>

using namespace ns3;
using namespace std;

inline map<uint32_t, map<uint32_t, uint32_t>> queue_result;

inline void monitor_buffer(FILE *qlen_output, NodeContainer *n,
                           uint32_t interval) {
  for (uint32_t i = 0; i < n->GetN(); i++) {
    if (n->Get(i)->GetNodeType() == 1) {
      Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(n->Get(i));
      if (queue_result.find(i) == queue_result.end())
        queue_result[i];
      int test = 0;
      for (uint32_t j = 1; j < sw->GetNDevices(); j++) {
        uint32_t size = 0;
        for (uint32_t k = 0; k < SwitchMmu::qCnt; k++)
          size += sw->m_mmu->egress_bytes[j][k];
        if (size >= 1000) {
          queue_result[i][j] = size;
          if (test == 0) {
            test = 1;
            fprintf(qlen_output, "time %lu %u ",
                    Simulator::Now().GetTimeStep(), i);
          }
          if (j < sw->GetNDevices() - 1) {
            test = 2;
            fprintf(qlen_output, "j %u %u ", j, size);
          } else if (j == sw->GetNDevices() - 1) {
            fprintf(qlen_output, "j %u %u\n", j, size);
            test = 3;
          }
        }
        if (j == sw->GetNDevices() - 1 && test == 2) {
          fprintf(qlen_output, "\n");
        }
      }
      fflush(qlen_output);
    }
  }
  fflush(qlen_output);
  Simulator::Schedule(NanoSeconds(interval), &monitor_buffer, qlen_output, n,
                      interval);
}

#endif // MONITORING_H
