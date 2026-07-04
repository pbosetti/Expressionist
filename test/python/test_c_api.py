import math

import pytest

from expressionist import EvalMethod, Expressionist, ExpressionistError


def test_basic_arithmetic():
    ex = Expressionist()
    out = ex.evaluate({"a": 1, "b": 2, "c": "$a + b"})
    assert out["c"] == 3


def test_both_eval_methods_agree():
    data = {"a": 1, "b": 2, "c": "$a + b", "f": "$pi/3"}
    for method in (EvalMethod.GRAPH, EvalMethod.RECURSIVE):
        ex = Expressionist(method=method)
        out = ex.evaluate(data)
        assert out["c"] == 3
        assert out["f"] == pytest.approx(math.pi / 3)


def test_input_is_not_mutated():
    data = {"a": 1, "b": "$a + 1"}
    Expressionist().evaluate(data)
    assert data["b"] == "$a + 1"


def test_custom_tag():
    ex = Expressionist()
    ex.set_tag("@")
    out = ex.evaluate({"a": 1, "b": "@a + 1", "c": "$a + 1"})
    assert out["b"] == 2
    assert out["c"] == "$a + 1"


def test_disable_key_skips_subtree():
    ex = Expressionist()
    out = ex.evaluate({"a": 1, "frozen": {"expressionist": False, "b": "$a + 1"}})
    assert out["frozen"]["b"] == "$a + 1"
    assert out["frozen"]["expressionist"] is False


def test_custom_disable_key():
    ex = Expressionist()
    ex.set_disable_key("skip")
    out = ex.evaluate({"a": 1, "frozen": {"skip": False, "b": "$a + 1"}})
    assert out["frozen"]["b"] == "$a + 1"


def test_undefined_variable_raises():
    ex = Expressionist()
    with pytest.raises(ExpressionistError):
        ex.evaluate({"a": "$missing + 1"})


def test_malformed_json_raises():
    ex = Expressionist()
    with pytest.raises(ExpressionistError):
        ex.evaluate("{not valid json")


def test_accepts_json_string_input():
    ex = Expressionist()
    out = ex.evaluate('{"a": 1, "b": "$a + 1"}')
    assert out["b"] == 2


def test_context_manager_closes_handle():
    with Expressionist() as ex:
        assert ex.evaluate({"a": 1})["a"] == 1
