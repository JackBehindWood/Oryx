#pragma once

#include "Oryx/Strategy/Observability/IDecisionObserver.h"

namespace oryx
{

constexpr int32_t k_trace_schema_version = 1;

// One JSON object per decision behind a {"schema_version": N} header line; the stream must outlive the writer.
class JsonLinesWriter : public IDecisionObserver
{
public:
    explicit JsonLinesWriter(std::ostream& out);

    void on_decision(const IState& state, const Decision& decision) override;

private:
    std::ostream& m_out;
    int32_t m_ply = 0;
};

} // namespace oryx
