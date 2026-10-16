#pragma once

#include "Oryx/Core/Params.h"
#include "Oryx/Strategy/Observability/ActionDescription.h"
#include "Oryx/Strategy/Observability/IDecisionObserver.h"
#include "Oryx/Strategy/Observability/TraceSchema.h"

namespace oryx
{

// Identity written on the header line so a log can be replayed without its producer.
struct TraceHeader
{
    std::string game;
    Params params;
    std::vector<std::string> players;
    uint64_t seed = 0;
    std::string build;
};

struct TraceOptions
{
    TraceHeader header;
    // Labels and board spaces in the board's vocabulary; null falls back to IState::action_to_string and no spaces.
    SharedPtr<const IActionDescriber> describer;
    // Adds t_us (microseconds since the match started); off keeps the output deterministic.
    bool record_time = false;
};

// One JSON object per line: a header, then a decision per line tagged with its match and ply; the stream must outlive the writer.
class JsonLinesWriter : public IDecisionObserver
{
public:
    explicit JsonLinesWriter(std::ostream& out, TraceOptions options = TraceOptions());

    void on_decision(const IState& state, const Decision& decision) override;
    void on_match_start() override;
    void on_match_end(const Outcome& outcome) override;

private:
    std::ostream& m_out;
    TraceOptions m_options;
    int32_t m_match = 0;
    int32_t m_ply = 0;
    std::chrono::steady_clock::time_point m_match_started;
};

} // namespace oryx
