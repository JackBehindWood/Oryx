#include "doctest.h"

#include "Oryx.h"

namespace
{

oryx::ParamSchema nim_schema()
{
    return {
        oryx::int_param("stones", 21),
        oryx::int_param("max_take", 3),
        oryx::double_param("scale", 0.5),
        oryx::bool_param("verbose", false),
        oryx::string_param("label", "nim"),
        oryx::param_without_default("seed", oryx::ParamType::Int),
    };
}

std::string error_message(const oryx::ParamSchema& schema, const oryx::Params& params)
{
    try
    {
        oryx::Params resolved = oryx::resolve_params("nim", schema, params);
    }
    catch (const oryx::ParamError& error)
    {
        return error.what();
    }
    return "";
}

} // namespace

TEST_CASE("resolve_params fills in every declared default")
{
    oryx::Params resolved = oryx::resolve_params("nim", nim_schema(), {});

    CHECK(oryx::get_param<int64_t>(resolved, "stones") == 21);
    CHECK(oryx::get_param<int64_t>(resolved, "max_take") == 3);
    CHECK(oryx::get_param<double>(resolved, "scale") == 0.5);
    CHECK(oryx::get_param<bool>(resolved, "verbose") == false);
    CHECK(oryx::get_param<std::string>(resolved, "label") == "nim");
}

TEST_CASE("resolve_params leaves a param without a default absent unless it is supplied")
{
    oryx::Params omitted = oryx::resolve_params("nim", nim_schema(), {});
    CHECK_FALSE(oryx::has_param(omitted, "seed"));

    oryx::Params supplied = oryx::resolve_params("nim", nim_schema(), { { "seed", int64_t{ 7 } } });
    REQUIRE(oryx::has_param(supplied, "seed"));
    CHECK(oryx::get_param<int64_t>(supplied, "seed") == 7);
}

TEST_CASE("resolve_params rejects a missing required param and accepts it once supplied")
{
    oryx::ParamSchema schema = { oryx::int_param("stones", 21), oryx::required_param("label", oryx::ParamType::String) };

    CHECK(error_message(schema, {}) == "'nim': parameter 'label' is required");

    oryx::Params resolved = oryx::resolve_params("nim", schema, { { "label", std::string("x") } });
    CHECK(oryx::get_param<std::string>(resolved, "label") == "x");
    CHECK(oryx::get_param<int64_t>(resolved, "stones") == 21);
}

TEST_CASE("resolve_params reports a mistyped required param before it reports a missing one")
{
    oryx::ParamSchema schema = { oryx::required_param("a", oryx::ParamType::Int), oryx::required_param("b", oryx::ParamType::Int) };

    CHECK(error_message(schema, { { "a", std::string("x") } }) == "'nim': parameter 'a' expects int but got string");
    CHECK(error_message(schema, { { "a", int64_t{ 1 } } }) == "'nim': parameter 'b' is required");
}

TEST_CASE("required_param is required, param_without_default is optional, and neither has a default")
{
    oryx::ParamSpec required = oryx::required_param("a", oryx::ParamType::Bool, "doc");
    oryx::ParamSpec optional = oryx::param_without_default("b", oryx::ParamType::Bool);

    CHECK(required.required);
    CHECK_FALSE(required.has_default);
    CHECK(required.description == "doc");
    CHECK_FALSE(optional.required);
    CHECK_FALSE(optional.has_default);
    CHECK_FALSE(oryx::int_param("c", 1).required);
}

TEST_CASE("required_param_names lists only the required params, in schema order")
{
    oryx::ParamSchema schema = { oryx::required_param("z", oryx::ParamType::Int), oryx::int_param("m", 1), oryx::param_without_default("n", oryx::ParamType::Int), oryx::required_param("a", oryx::ParamType::Bool) };

    CHECK(oryx::required_param_names(schema) == std::vector<std::string>{ "z", "a" });
    CHECK(oryx::required_param_names(nim_schema()).empty());
    CHECK(oryx::required_param_names({}).empty());
}

TEST_CASE("resolve_params lets supplied values override defaults")
{
    oryx::Params resolved = oryx::resolve_params("nim", nim_schema(), { { "stones", int64_t{ 15 } }, { "label", std::string("custom") } });

    CHECK(oryx::get_param<int64_t>(resolved, "stones") == 15);
    CHECK(oryx::get_param<std::string>(resolved, "label") == "custom");
    CHECK(oryx::get_param<int64_t>(resolved, "max_take") == 3);
}

TEST_CASE("resolve_params widens an int to a double param, and nothing else")
{
    oryx::Params widened = oryx::resolve_params("nim", nim_schema(), { { "scale", int64_t{ 2 } } });
    CHECK(oryx::get_param<double>(widened, "scale") == 2.0);

    CHECK_THROWS_AS(oryx::resolve_params("nim", nim_schema(), { { "stones", 3.0 } }), oryx::ParamError);
    CHECK_THROWS_AS(oryx::resolve_params("nim", nim_schema(), { { "stones", true } }), oryx::ParamError);
    CHECK_THROWS_AS(oryx::resolve_params("nim", nim_schema(), { { "verbose", int64_t{ 1 } } }), oryx::ParamError);
}

TEST_CASE("resolve_params names the unknown key, the entry and the known keys")
{
    std::string message = error_message(nim_schema(), { { "stonez", int64_t{ 3 } } });

    CHECK(message.find("'nim'") != std::string::npos);
    CHECK(message.find("'stonez'") != std::string::npos);
    CHECK(message.find("stones") != std::string::npos);
    CHECK(message.find("max_take") != std::string::npos);
}

TEST_CASE("resolve_params reports an empty schema's unknown key")
{
    std::string message = error_message({}, { { "anything", int64_t{ 1 } } });

    CHECK(message.find("'anything'") != std::string::npos);
    CHECK(message.find("none") != std::string::npos);
}

TEST_CASE("resolve_params names the mistyped key with the expected and given types")
{
    std::string message = error_message(nim_schema(), { { "stones", std::string("many") } });

    CHECK(message.find("'stones'") != std::string::npos);
    CHECK(message.find("int") != std::string::npos);
    CHECK(message.find("string") != std::string::npos);
}

TEST_CASE("param_type_of and to_string agree with the variant alternatives")
{
    CHECK(oryx::param_type_of(oryx::ParamValue{ true }) == oryx::ParamType::Bool);
    CHECK(oryx::param_type_of(oryx::ParamValue{ int64_t{ 1 } }) == oryx::ParamType::Int);
    CHECK(oryx::param_type_of(oryx::ParamValue{ 1.0 }) == oryx::ParamType::Double);
    CHECK(oryx::param_type_of(oryx::ParamValue{ std::string("x") }) == oryx::ParamType::String);
    CHECK(oryx::to_string(oryx::ParamType::Double) == "double");
}
