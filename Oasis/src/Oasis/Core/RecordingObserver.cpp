#include "RecordingObserver.h"

namespace oasis
{

RecordingObserver::RecordingObserver(std::string path)
    : m_path(std::move(path))
{
}

void RecordingObserver::begin_game(const std::string& game, oryx::SharedPtr<const oryx::IActionDescriber> describer)
{
    m_writer.reset();
    m_file.close();
    m_game = game;
    m_describer = std::move(describer);
}

void RecordingObserver::open_segment()
{
    std::filesystem::path path(m_path);
    if (m_segment > 0)
    {
        path.replace_filename(path.stem().string() + "-" + std::to_string(m_segment + 1) + path.extension().string());
    }
    m_file.open(path, std::ios::out | std::ios::trunc);
    if (!m_file)
    {
        throw oryx::Error("--record: cannot open '" + path.string() + "'");
    }
    ++m_segment;
    m_current_path = path.string();

    oryx::TraceOptions options;
    options.header.game = m_game;
    options.header.players = { "player", m_opponent };
    options.header.build = oryx::build_info().version + " " + oryx::build_info().git_hash + " " + oryx::build_info().profile;
    options.describer = m_describer;
    options.record_time = true;
    m_writer = oryx::create_unique<oryx::JsonLinesWriter>(m_file, std::move(options));
}

void RecordingObserver::on_decision(const oryx::IState& state, const oryx::Decision& decision)
{
    if (m_writer == nullptr)
    {
        open_segment();
    }
    m_writer->on_decision(state, decision);
}

void RecordingObserver::on_match_start()
{
    if (m_writer != nullptr)
    {
        m_file.flush();
        m_writer->on_match_start();
    }
}

void RecordingObserver::on_match_end(const oryx::Outcome& outcome)
{
    if (m_writer != nullptr)
    {
        m_writer->on_match_end(outcome);
        m_file.flush();
    }
}

} // namespace oasis
