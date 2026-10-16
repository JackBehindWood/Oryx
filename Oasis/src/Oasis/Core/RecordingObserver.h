#pragma once

#include "Oryx.h"

namespace oasis
{

// Writes every decision of the interactive match to a JSON-lines trace; a game switch starts the next segment (`name-2.jsonl`) because a log holds one game.
class RecordingObserver final : public oryx::IDecisionObserver
{
public:
    explicit RecordingObserver(std::string path);

    // The game the next decisions belong to; the segment opens lazily at its first decision.
    void begin_game(const std::string& game, oryx::SharedPtr<const oryx::IActionDescriber> describer);
    void set_opponent(const std::string& opponent) { m_opponent = opponent; }

    void on_decision(const oryx::IState& state, const oryx::Decision& decision) override;
    void on_match_start() override;
    void on_match_end(const oryx::Outcome& outcome) override;

    [[nodiscard]] const std::string& current_path() const { return m_current_path; }

private:
    void open_segment();

    std::string m_path;
    std::string m_game;
    std::string m_opponent;
    std::string m_current_path;
    oryx::SharedPtr<const oryx::IActionDescriber> m_describer;
    std::ofstream m_file;
    oryx::UniquePtr<oryx::JsonLinesWriter> m_writer;
    int32_t m_segment = 0;
};

} // namespace oasis
