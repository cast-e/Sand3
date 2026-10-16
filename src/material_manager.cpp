#include "material_manager.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

#include "grid.hpp"
#include "set_manager.hpp"
#include "vulkan.hpp"

namespace fs = std::filesystem;

std::vector<MaterialDefinition> MaterialManager::materials{};
std::array<RuntimeMaterial, 256> MaterialManager::runtime_materials{};
std::array<uint8_t, 256> MaterialManager::material_by_id{};
MaterialDefinition MaterialManager::default_empty{"empty", 255, 255, {64, 64, 64}};
RuntimeMaterial MaterialManager::default_runtime_empty{255, MaterialManager::pack_color({64, 64, 64}), {}};

uint32_t MaterialManager::pack_color(const std::array<uint8_t, 3>& color) {
	return (255u << 24) | (color[2] << 16) | (color[1] << 8) | color[0];
}

uint8_t MaterialManager::get_unused_id() {
	std::array<bool, 256> used{};
	used[0] = true;
	used[255] = true;

	for (const auto& m : materials) {
		if (m.id < 255) {
			used[m.id] = true;
		}
	}

	for (int i = 1; i < 255; ++i) {
		if (!used[i]) {
			return static_cast<uint8_t>(i);
		}
	}

	return 254;
}

bool MaterialManager::is_valid_name(std::string_view name) {
	if (name.empty()) {
		return false;
	}
	return std::ranges::all_of(materials, [&](const auto& m) { return m.name != name; });
}

bool parse_and_compile_expr(const std::string& expr, uint8_t target_attr, std::vector<ExprOp>& out_ops,
							std::string* error_out, int cell_dx, int cell_dy) {
	out_ops.clear();
	auto set_err = [&](std::string_view msg) {
		if (error_out)
			*error_out = std::string(msg);
		return false;
	};

	size_t start_idx = 0;
	while (start_idx < expr.size() && std::isspace(static_cast<unsigned char>(expr[start_idx])))
		start_idx++;
	size_t end_idx = expr.size();
	while (end_idx > start_idx && std::isspace(static_cast<unsigned char>(expr[end_idx - 1])))
		end_idx--;
	if (start_idx >= end_idx) {
		return set_err("Empty expression");
	}

	const auto& attributes = SetManager::get_current_metadata().attributes;
	auto get_attr_name = [&](size_t idx) -> std::string {
		if (idx < attributes.size())
			return attributes[idx].name;
		return "";
	};
	std::string attr0 = get_attr_name(0);
	std::string attr1 = get_attr_name(1);
	std::string attr2 = get_attr_name(2);
	std::string cur_attr_name = get_attr_name(target_attr);

	enum class TokenType {
		Number,
		LoadSelf,
		LoadCenter,
		LoadMin,
		LoadMax,
		LoadCellAttr,
		OpAdd,
		OpSub,
		OpMul,
		OpDiv,
		OpMod,
		OpPow,
		OpNeg,
		OpenParen,
		CloseParen,
		Comma,
		FuncAbs,
		FuncMin,
		FuncMax,
		FuncClamp,
		FuncFloor,
		FuncCeil,
		FuncRound
	};

	struct Token {
		TokenType type;
		float num_val = 0.0f;
		int8_t dx = 0;
		int8_t dy = 0;
		uint8_t attr = 0;
	};

	std::vector<Token> tokens;
	size_t i = start_idx;
	bool can_be_unary = true;
	int func_paren_depth = 0;

	while (i < end_idx) {
		char c = expr[i];
		if (std::isspace(static_cast<unsigned char>(c))) {
			i++;
			continue;
		}

		if (c == '~' && i + 1 < end_idx && expr[i + 1] == '[') {
			size_t close_bracket = expr.find(']', i + 1);
			if (close_bracket != std::string::npos && close_bracket < end_idx) {
				std::string inner = expr.substr(i + 2, close_bracket - i - 2);
				size_t comma_pos = inner.find(',');
				if (comma_pos != std::string::npos) {
					try {
						int rdx = std::stoi(inner.substr(0, comma_pos));
						int rdy = std::stoi(inner.substr(comma_pos + 1));
						tokens.push_back({TokenType::LoadCellAttr, 0.0f, static_cast<int8_t>(cell_dx + rdx),
										  static_cast<int8_t>(cell_dy + rdy), target_attr});
						i = close_bracket + 1;
						can_be_unary = false;
						continue;
					} catch (...) {}
				}
			}
		}

		if (c == '[') {
			size_t close_bracket = expr.find(']', i);
			if (close_bracket != std::string::npos && close_bracket < end_idx) {
				std::string inner = expr.substr(i + 1, close_bracket - i - 1);
				size_t comma_pos = inner.find(',');
				if (comma_pos != std::string::npos) {
					try {
						int dx = std::stoi(inner.substr(0, comma_pos));
						int dy = std::stoi(inner.substr(comma_pos + 1));
						tokens.push_back({TokenType::LoadCellAttr, 0.0f, static_cast<int8_t>(dx),
										  static_cast<int8_t>(dy), target_attr});
						i = close_bracket + 1;
						can_be_unary = false;
						continue;
					} catch (...) {}
				}
			}
		}

		bool is_func_call = !tokens.empty() &&
							(tokens.back().type == TokenType::FuncAbs || tokens.back().type == TokenType::FuncMin ||
							 tokens.back().type == TokenType::FuncMax || tokens.back().type == TokenType::FuncClamp ||
							 tokens.back().type == TokenType::FuncFloor || tokens.back().type == TokenType::FuncCeil ||
							 tokens.back().type == TokenType::FuncRound);

		if (c == '(' && !is_func_call) {
			size_t close_paren = expr.find(')', i);
			if (close_paren != std::string::npos && close_paren < end_idx) {
				std::string inner = expr.substr(i + 1, close_paren - i - 1);
				size_t comma_pos = inner.find(',');
				if (comma_pos != std::string::npos) {
					bool valid_coord = true;
					for (char ic : inner) {
						if (!std::isdigit(static_cast<unsigned char>(ic)) && ic != '+' && ic != '-' && ic != ' ' &&
							ic != '\t' && ic != ',') {
							valid_coord = false;
							break;
						}
					}
					if (valid_coord) {
						try {
							int dx = std::stoi(inner.substr(0, comma_pos));
							int dy = std::stoi(inner.substr(comma_pos + 1));
							tokens.push_back({TokenType::LoadCellAttr, 0.0f, static_cast<int8_t>(dx),
											  static_cast<int8_t>(dy), target_attr});
							i = close_paren + 1;
							can_be_unary = false;
							continue;
						} catch (...) {}
					}
				}
			}
		}

		if (func_paren_depth == 0 && (std::isdigit(static_cast<unsigned char>(c)) ||
									  ((c == '+' || c == '-') && can_be_unary && i + 1 < end_idx &&
									   std::isdigit(static_cast<unsigned char>(expr[i + 1]))))) {
			size_t j = i;
			if (expr[j] == '+' || expr[j] == '-')
				j++;
			while (j < end_idx && std::isdigit(static_cast<unsigned char>(expr[j])))
				j++;
			if (j < end_idx && expr[j] == ',') {
				size_t k = j + 1;
				while (k < end_idx && std::isspace(static_cast<unsigned char>(expr[k])))
					k++;
				if (k < end_idx && (std::isdigit(static_cast<unsigned char>(expr[k])) ||
									((expr[k] == '+' || expr[k] == '-') && k + 1 < end_idx &&
									 std::isdigit(static_cast<unsigned char>(expr[k + 1]))))) {
					size_t l = k;
					if (expr[l] == '+' || expr[l] == '-')
						l++;
					while (l < end_idx && std::isdigit(static_cast<unsigned char>(expr[l])))
						l++;
					if (l >= end_idx || expr[l] != '.') {
						try {
							int dx = std::stoi(expr.substr(i, j - i));
							int dy = std::stoi(expr.substr(k, l - k));
							tokens.push_back({TokenType::LoadCellAttr, 0.0f, static_cast<int8_t>(dx),
											  static_cast<int8_t>(dy), target_attr});
							i = l;
							can_be_unary = false;
							continue;
						} catch (...) {}
					}
				}
			}
		}

		if (c == '-' && can_be_unary) {
			tokens.push_back({TokenType::OpNeg});
			i++;
			can_be_unary = true;
			continue;
		}
		if (c == '+' && can_be_unary) {
			i++;
			continue;
		}

		if (c == '+') {
			tokens.push_back({TokenType::OpAdd});
			i++;
			can_be_unary = true;
			continue;
		}
		if (c == '-') {
			tokens.push_back({TokenType::OpSub});
			i++;
			can_be_unary = true;
			continue;
		}
		if (c == '*') {
			tokens.push_back({TokenType::OpMul});
			i++;
			can_be_unary = true;
			continue;
		}
		if (c == '/') {
			tokens.push_back({TokenType::OpDiv});
			i++;
			can_be_unary = true;
			continue;
		}
		if (c == '%') {
			tokens.push_back({TokenType::OpMod});
			i++;
			can_be_unary = true;
			continue;
		}
		if (c == '^') {
			tokens.push_back({TokenType::OpPow});
			i++;
			can_be_unary = true;
			continue;
		}
		if (c == '(') {
			tokens.push_back({TokenType::OpenParen});
			i++;
			can_be_unary = true;
			continue;
		}
		if (c == ')') {
			tokens.push_back({TokenType::CloseParen});
			i++;
			can_be_unary = false;
			if (func_paren_depth > 0)
				func_paren_depth--;
			continue;
		}
		if (c == ',') {
			tokens.push_back({TokenType::Comma});
			i++;
			can_be_unary = true;
			continue;
		}

		if (std::isdigit(static_cast<unsigned char>(c)) ||
			(c == '.' && i + 1 < end_idx && std::isdigit(static_cast<unsigned char>(expr[i + 1])))) {
			size_t num_start = i;
			while (i < end_idx && (std::isdigit(static_cast<unsigned char>(expr[i])) || expr[i] == '.'))
				i++;
			float val = 0.0f;
			try {
				val = std::stof(expr.substr(num_start, i - num_start));
			} catch (...) {
				return set_err(fmt::format("Invalid number: {}", expr.substr(num_start, i - num_start)));
			}
			tokens.push_back({TokenType::Number, val});
			can_be_unary = false;
			continue;
		}

		if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
			size_t id_start = i;
			while (i < end_idx &&
				   (std::isalnum(static_cast<unsigned char>(expr[i])) || expr[i] == '_' || expr[i] == '.'))
				i++;
			std::string id = expr.substr(id_start, i - id_start);

			size_t peek = i;
			while (peek < end_idx && std::isspace(static_cast<unsigned char>(expr[peek])))
				peek++;
			bool has_open_paren = (peek < end_idx && expr[peek] == '(');

			if (id == "abs" && has_open_paren) {
				tokens.push_back({TokenType::FuncAbs});
				func_paren_depth++;
				can_be_unary = true;
			} else if (id == "min" && has_open_paren) {
				tokens.push_back({TokenType::FuncMin});
				func_paren_depth++;
				can_be_unary = true;
			} else if (id == "max" && has_open_paren) {
				tokens.push_back({TokenType::FuncMax});
				func_paren_depth++;
				can_be_unary = true;
			} else if (id == "clamp" && has_open_paren) {
				tokens.push_back({TokenType::FuncClamp});
				func_paren_depth++;
				can_be_unary = true;
			} else if (id == "floor" && has_open_paren) {
				tokens.push_back({TokenType::FuncFloor});
				func_paren_depth++;
				can_be_unary = true;
			} else if (id == "ceil" && has_open_paren) {
				tokens.push_back({TokenType::FuncCeil});
				func_paren_depth++;
				can_be_unary = true;
			} else if (id == "round" && has_open_paren) {
				tokens.push_back({TokenType::FuncRound});
				func_paren_depth++;
				can_be_unary = true;
			} else if (id == "m" || id == "min" ||
					   (!cur_attr_name.empty() && (id == cur_attr_name + ".min" || id == "min." + cur_attr_name))) {
				tokens.push_back({TokenType::LoadMin, 0.0f, 0, 0, target_attr});
				can_be_unary = false;
			} else if (id == "m0" || id == "min0" ||
					   (!attr0.empty() && (id == attr0 + ".min" || id == "min." + attr0 || id == "m." + attr0))) {
				tokens.push_back({TokenType::LoadMin, 0.0f, 0, 0, 0});
				can_be_unary = false;
			} else if (id == "m1" || id == "min1" ||
					   (!attr1.empty() && (id == attr1 + ".min" || id == "min." + attr1 || id == "m." + attr1))) {
				tokens.push_back({TokenType::LoadMin, 0.0f, 0, 0, 1});
				can_be_unary = false;
			} else if (id == "m2" || id == "min2" ||
					   (!attr2.empty() && (id == attr2 + ".min" || id == "min." + attr2 || id == "m." + attr2))) {
				tokens.push_back({TokenType::LoadMin, 0.0f, 0, 0, 2});
				can_be_unary = false;
			} else if (id == "M" || id == "max" ||
					   (!cur_attr_name.empty() && (id == cur_attr_name + ".max" || id == "max." + cur_attr_name))) {
				tokens.push_back({TokenType::LoadMax, 0.0f, 0, 0, target_attr});
				can_be_unary = false;
			} else if (id == "M0" || id == "max0" ||
					   (!attr0.empty() && (id == attr0 + ".max" || id == "max." + attr0 || id == "M." + attr0))) {
				tokens.push_back({TokenType::LoadMax, 0.0f, 0, 0, 0});
				can_be_unary = false;
			} else if (id == "M1" || id == "max1" ||
					   (!attr1.empty() && (id == attr1 + ".max" || id == "max." + attr1 || id == "M." + attr1))) {
				tokens.push_back({TokenType::LoadMax, 0.0f, 0, 0, 1});
				can_be_unary = false;
			} else if (id == "M2" || id == "max2" ||
					   (!attr2.empty() && (id == attr2 + ".max" || id == "max." + attr2 || id == "M." + attr2))) {
				tokens.push_back({TokenType::LoadMax, 0.0f, 0, 0, 2});
				can_be_unary = false;
			} else {
				return set_err(fmt::format("Unknown identifier: '{}'", id));
			}
			continue;
		}

		return set_err(fmt::format("Unexpected character: '{}'", c));
	}

	std::vector<Token> op_stack;
	std::vector<int> func_arg_counts;

	auto is_func = [](TokenType t) {
		return t == TokenType::FuncAbs || t == TokenType::FuncMin || t == TokenType::FuncMax ||
			   t == TokenType::FuncClamp || t == TokenType::FuncFloor || t == TokenType::FuncCeil ||
			   t == TokenType::FuncRound;
	};

	auto precedence = [](TokenType t) -> int {
		switch (t) {
			case TokenType::OpNeg:
				return 5;
			case TokenType::OpPow:
				return 4;
			case TokenType::OpMul:
			case TokenType::OpDiv:
			case TokenType::OpMod:
				return 3;
			case TokenType::OpAdd:
			case TokenType::OpSub:
				return 2;
			default:
				return 0;
		}
	};

	auto is_right_assoc = [](TokenType t) -> bool { return t == TokenType::OpNeg || t == TokenType::OpPow; };

	for (const auto& tok : tokens) {
		switch (tok.type) {
			case TokenType::Number:
				out_ops.push_back({ExprOpType::PushConst, 0, 0, 0, tok.num_val});
				break;
			case TokenType::LoadSelf:
				out_ops.push_back({ExprOpType::LoadSelf, 0, 0, tok.attr, 0.0f});
				break;
			case TokenType::LoadCenter:
				out_ops.push_back({ExprOpType::LoadCenter, 0, 0, tok.attr, 0.0f});
				break;
			case TokenType::LoadMin:
				out_ops.push_back({ExprOpType::LoadMin, 0, 0, tok.attr, 0.0f});
				break;
			case TokenType::LoadMax:
				out_ops.push_back({ExprOpType::LoadMax, 0, 0, tok.attr, 0.0f});
				break;
			case TokenType::LoadCellAttr:
				out_ops.push_back({ExprOpType::LoadCellAttr, tok.dx, tok.dy, tok.attr, 0.0f});
				break;
			case TokenType::FuncAbs:
			case TokenType::FuncMin:
			case TokenType::FuncMax:
			case TokenType::FuncClamp:
			case TokenType::FuncFloor:
			case TokenType::FuncCeil:
			case TokenType::FuncRound:
				op_stack.push_back(tok);
				func_arg_counts.push_back(1);
				break;
			case TokenType::Comma: {
				while (!op_stack.empty() && op_stack.back().type != TokenType::OpenParen) {
					auto top = op_stack.back();
					op_stack.pop_back();
					if (top.type == TokenType::OpAdd)
						out_ops.push_back({ExprOpType::Add});
					else if (top.type == TokenType::OpSub)
						out_ops.push_back({ExprOpType::Sub});
					else if (top.type == TokenType::OpMul)
						out_ops.push_back({ExprOpType::Mul});
					else if (top.type == TokenType::OpDiv)
						out_ops.push_back({ExprOpType::Div});
					else if (top.type == TokenType::OpMod)
						out_ops.push_back({ExprOpType::Mod});
					else if (top.type == TokenType::OpPow)
						out_ops.push_back({ExprOpType::Pow});
					else if (top.type == TokenType::OpNeg)
						out_ops.push_back({ExprOpType::Neg});
				}
				if (op_stack.empty()) {
					return set_err("Misplaced comma outside parentheses");
				}
				if (!func_arg_counts.empty()) {
					func_arg_counts.back()++;
				}
				break;
			}
			case TokenType::OpenParen:
				op_stack.push_back(tok);
				break;
			case TokenType::CloseParen: {
				while (!op_stack.empty() && op_stack.back().type != TokenType::OpenParen) {
					auto top = op_stack.back();
					op_stack.pop_back();
					if (top.type == TokenType::OpAdd)
						out_ops.push_back({ExprOpType::Add});
					else if (top.type == TokenType::OpSub)
						out_ops.push_back({ExprOpType::Sub});
					else if (top.type == TokenType::OpMul)
						out_ops.push_back({ExprOpType::Mul});
					else if (top.type == TokenType::OpDiv)
						out_ops.push_back({ExprOpType::Div});
					else if (top.type == TokenType::OpMod)
						out_ops.push_back({ExprOpType::Mod});
					else if (top.type == TokenType::OpPow)
						out_ops.push_back({ExprOpType::Pow});
					else if (top.type == TokenType::OpNeg)
						out_ops.push_back({ExprOpType::Neg});
				}
				if (op_stack.empty()) {
					return set_err("Mismatched closing parenthesis");
				}
				op_stack.pop_back();
				if (!op_stack.empty() && is_func(op_stack.back().type)) {
					auto func_tok = op_stack.back();
					op_stack.pop_back();
					int args = func_arg_counts.empty() ? 0 : func_arg_counts.back();
					if (!func_arg_counts.empty())
						func_arg_counts.pop_back();

					if (func_tok.type == TokenType::FuncAbs) {
						if (args != 1)
							return set_err("abs() expects 1 argument");
						out_ops.push_back({ExprOpType::Abs});
					} else if (func_tok.type == TokenType::FuncFloor) {
						if (args != 1)
							return set_err("floor() expects 1 argument");
						out_ops.push_back({ExprOpType::Floor});
					} else if (func_tok.type == TokenType::FuncCeil) {
						if (args != 1)
							return set_err("ceil() expects 1 argument");
						out_ops.push_back({ExprOpType::Ceil});
					} else if (func_tok.type == TokenType::FuncRound) {
						if (args != 1)
							return set_err("round() expects 1 argument");
						out_ops.push_back({ExprOpType::Round});
					} else if (func_tok.type == TokenType::FuncMin) {
						if (args != 2)
							return set_err("min() expects 2 arguments");
						out_ops.push_back({ExprOpType::Min});
					} else if (func_tok.type == TokenType::FuncMax) {
						if (args != 2)
							return set_err("max() expects 2 arguments");
						out_ops.push_back({ExprOpType::Max});
					} else if (func_tok.type == TokenType::FuncClamp) {
						if (args != 3)
							return set_err("clamp() expects 3 arguments (x, lo, hi)");
						out_ops.push_back({ExprOpType::Clamp});
					}
				}
				break;
			}
			case TokenType::OpAdd:
			case TokenType::OpSub:
			case TokenType::OpMul:
			case TokenType::OpDiv:
			case TokenType::OpMod:
			case TokenType::OpPow:
			case TokenType::OpNeg: {
				int p1 = precedence(tok.type);
				bool r_assoc = is_right_assoc(tok.type);
				while (!op_stack.empty() && op_stack.back().type != TokenType::OpenParen) {
					TokenType top = op_stack.back().type;
					int p2 = precedence(top);
					if (is_func(top) || p2 > p1 || (p2 == p1 && !r_assoc)) {
						op_stack.pop_back();
						if (top == TokenType::OpAdd)
							out_ops.push_back({ExprOpType::Add});
						else if (top == TokenType::OpSub)
							out_ops.push_back({ExprOpType::Sub});
						else if (top == TokenType::OpMul)
							out_ops.push_back({ExprOpType::Mul});
						else if (top == TokenType::OpDiv)
							out_ops.push_back({ExprOpType::Div});
						else if (top == TokenType::OpMod)
							out_ops.push_back({ExprOpType::Mod});
						else if (top == TokenType::OpPow)
							out_ops.push_back({ExprOpType::Pow});
						else if (top == TokenType::OpNeg)
							out_ops.push_back({ExprOpType::Neg});
					} else {
						break;
					}
				}
				op_stack.push_back(tok);
				break;
			}
		}
	}

	while (!op_stack.empty()) {
		auto top = op_stack.back();
		op_stack.pop_back();
		if (top.type == TokenType::OpenParen) {
			return set_err("Mismatched open parenthesis");
		}
		if (top.type == TokenType::OpAdd)
			out_ops.push_back({ExprOpType::Add});
		else if (top.type == TokenType::OpSub)
			out_ops.push_back({ExprOpType::Sub});
		else if (top.type == TokenType::OpMul)
			out_ops.push_back({ExprOpType::Mul});
		else if (top.type == TokenType::OpDiv)
			out_ops.push_back({ExprOpType::Div});
		else if (top.type == TokenType::OpMod)
			out_ops.push_back({ExprOpType::Mod});
		else if (top.type == TokenType::OpPow)
			out_ops.push_back({ExprOpType::Pow});
		else if (top.type == TokenType::OpNeg)
			out_ops.push_back({ExprOpType::Neg});
	}

	int depth = 0;
	for (const auto& op : out_ops) {
		switch (op.type) {
			case ExprOpType::PushConst:
			case ExprOpType::LoadSelf:
			case ExprOpType::LoadCenter:
			case ExprOpType::LoadMin:
			case ExprOpType::LoadMax:
			case ExprOpType::LoadCellAttr:
				depth++;
				break;
			case ExprOpType::Neg:
			case ExprOpType::Abs:
			case ExprOpType::Floor:
			case ExprOpType::Ceil:
			case ExprOpType::Round:
				if (depth < 1)
					return set_err("Syntax error: missing operand for unary operator/function");
				break;
			case ExprOpType::Add:
			case ExprOpType::Sub:
			case ExprOpType::Mul:
			case ExprOpType::Div:
			case ExprOpType::Mod:
			case ExprOpType::Pow:
			case ExprOpType::Min:
			case ExprOpType::Max:
				if (depth < 2)
					return set_err("Syntax error: missing operand for binary operator/function");
				depth--;
				break;
			case ExprOpType::Clamp:
				if (depth < 3)
					return set_err("Syntax error: missing operands for clamp()");
				depth -= 2;
				break;
		}
	}
	if (depth != 1) {
		return set_err("Syntax error: incomplete or invalid expression");
	}
	return true;
}

std::string transform_expr_coords(const std::string& expr, int mode) {
	std::string res;
	res.reserve(expr.size());
	size_t i = 0;
	size_t n = expr.size();
	int func_paren_depth = 0;

	auto transform_pt = [mode](int dx, int dy) -> std::pair<int, int> {
		if (mode == 0)
			return {-dx, dy};
		if (mode == 1)
			return {dx, -dy};
		if (mode == 2)
			return {-dy, dx};
		if (mode == 3)
			return {dy, -dx};
		return {dx, dy};
	};

	while (i < n) {
		char c = expr[i];

		if (c == ')') {
			if (func_paren_depth > 0)
				func_paren_depth--;
			res += c;
			i++;
			continue;
		}

		if (c == '~' && i + 1 < n && expr[i + 1] == '[') {
			size_t close = expr.find(']', i + 1);
			if (close != std::string::npos) {
				std::string inner = expr.substr(i + 2, close - i - 2);
				size_t comma = inner.find(',');
				if (comma != std::string::npos) {
					try {
						int dx = std::stoi(inner.substr(0, comma));
						int dy = std::stoi(inner.substr(comma + 1));
						auto [ndx, ndy] = transform_pt(dx, dy);
						res += fmt::format("~[{},{}]", ndx, ndy);
						i = close + 1;
						continue;
					} catch (...) {}
				}
			}
		}

		if (c == '[') {
			size_t close = expr.find(']', i);
			if (close != std::string::npos) {
				std::string inner = expr.substr(i + 1, close - i - 1);
				size_t comma = inner.find(',');
				if (comma != std::string::npos) {
					try {
						int dx = std::stoi(inner.substr(0, comma));
						int dy = std::stoi(inner.substr(comma + 1));
						auto [ndx, ndy] = transform_pt(dx, dy);
						res += fmt::format("[{},{}]", ndx, ndy);
						i = close + 1;
						continue;
					} catch (...) {}
				}
			}
		}

		if (c == '(') {
			bool is_func_call = false;
			if (!res.empty()) {
				size_t prev = res.size();
				while (prev > 0 && std::isspace(static_cast<unsigned char>(res[prev - 1])))
					prev--;
				if (prev > 0 && (std::isalnum(static_cast<unsigned char>(res[prev - 1])) || res[prev - 1] == '_')) {
					is_func_call = true;
				}
			}

			if (is_func_call) {
				func_paren_depth++;
				res += c;
				i++;
				continue;
			}

			size_t close = expr.find(')', i);
			if (close != std::string::npos) {
				std::string inner = expr.substr(i + 1, close - i - 1);
				size_t comma = inner.find(',');
				if (comma != std::string::npos) {
					bool only_nums = true;
					for (char ic : inner) {
						if (!std::isdigit(static_cast<unsigned char>(ic)) && ic != '+' && ic != '-' && ic != ' ' &&
							ic != '\t' && ic != ',') {
							only_nums = false;
							break;
						}
					}
					if (only_nums) {
						try {
							int dx = std::stoi(inner.substr(0, comma));
							int dy = std::stoi(inner.substr(comma + 1));
							auto [ndx, ndy] = transform_pt(dx, dy);
							res += fmt::format("({},{})", ndx, ndy);
							i = close + 1;
							continue;
						} catch (...) {}
					}
				}
			}
		}

		if (func_paren_depth == 0 &&
			(std::isdigit(static_cast<unsigned char>(c)) ||
			 ((c == '+' || c == '-') && i + 1 < n && std::isdigit(static_cast<unsigned char>(expr[i + 1]))))) {
			size_t j = i;
			if (expr[j] == '+' || expr[j] == '-')
				j++;
			while (j < n && std::isdigit(static_cast<unsigned char>(expr[j])))
				j++;
			if (j < n && expr[j] == ',') {
				size_t k = j + 1;
				while (k < n && std::isspace(static_cast<unsigned char>(expr[k])))
					k++;
				if (k < n && (std::isdigit(static_cast<unsigned char>(expr[k])) ||
							  ((expr[k] == '+' || expr[k] == '-') && k + 1 < n &&
							   std::isdigit(static_cast<unsigned char>(expr[k + 1]))))) {
					size_t l = k;
					if (expr[l] == '+' || expr[l] == '-')
						l++;
					while (l < n && std::isdigit(static_cast<unsigned char>(expr[l])))
						l++;
					if (l >= n || expr[l] != '.') {
						try {
							int dx = std::stoi(expr.substr(i, j - i));
							int dy = std::stoi(expr.substr(k, l - k));
							auto [ndx, ndy] = transform_pt(dx, dy);
							res += fmt::format("{},{}", ndx, ndy);
							i = l;
							continue;
						} catch (...) {}
					}
				}
			}
		}

		res += c;
		i++;
	}
	return res;
}

std::string shift_expr_coords(const std::string& expr, int delta_x, int delta_y) {
	(void)delta_x;
	(void)delta_y;
	return expr;
}

float eval_expr(const std::vector<ExprOp>& ops, float self_val, float center_val, float min_val, float max_val,
				uint8_t cur_attr, const std::function<float(int8_t dx, int8_t dy, uint8_t attr)>& get_cell_attr) {
	if (ops.empty())
		return self_val;
	std::vector<float> stack;
	stack.reserve(16);
	for (const auto& op : ops) {
		switch (op.type) {
			case ExprOpType::PushConst:
				stack.push_back(op.val);
				break;
			case ExprOpType::LoadSelf:
				stack.push_back(self_val);
				break;
			case ExprOpType::LoadCenter:
				stack.push_back(center_val);
				break;
			case ExprOpType::LoadCellAttr: {
				uint8_t a = (op.attr < 3) ? op.attr : cur_attr;
				stack.push_back(get_cell_attr ? get_cell_attr(op.dx, op.dy, a) : 0.0f);
				break;
			}
			case ExprOpType::LoadMin:
				stack.push_back(min_val);
				break;
			case ExprOpType::LoadMax:
				stack.push_back(max_val);
				break;
			case ExprOpType::Add: {
				if (stack.size() >= 2) {
					float b = stack.back();
					stack.pop_back();
					stack.back() += b;
				}
				break;
			}
			case ExprOpType::Sub: {
				if (stack.size() >= 2) {
					float b = stack.back();
					stack.pop_back();
					stack.back() -= b;
				}
				break;
			}
			case ExprOpType::Mul: {
				if (stack.size() >= 2) {
					float b = stack.back();
					stack.pop_back();
					stack.back() *= b;
				}
				break;
			}
			case ExprOpType::Div: {
				if (stack.size() >= 2) {
					float b = stack.back();
					stack.pop_back();
					stack.back() = (b != 0.0f) ? (stack.back() / b) : 0.0f;
				}
				break;
			}
			case ExprOpType::Mod: {
				if (stack.size() >= 2) {
					float b = stack.back();
					stack.pop_back();
					stack.back() = (b != 0.0f) ? std::fmod(stack.back(), b) : 0.0f;
				}
				break;
			}
			case ExprOpType::Pow: {
				if (stack.size() >= 2) {
					float b = stack.back();
					stack.pop_back();
					float a = stack.back();
					stack.back() = (a >= 0.0f || std::floor(b) == b) ? std::pow(a, b) : 0.0f;
				}
				break;
			}
			case ExprOpType::Neg: {
				if (!stack.empty())
					stack.back() = -stack.back();
				break;
			}
			case ExprOpType::Abs: {
				if (!stack.empty())
					stack.back() = std::abs(stack.back());
				break;
			}
			case ExprOpType::Min: {
				if (stack.size() >= 2) {
					float b = stack.back();
					stack.pop_back();
					stack.back() = std::min(stack.back(), b);
				}
				break;
			}
			case ExprOpType::Max: {
				if (stack.size() >= 2) {
					float b = stack.back();
					stack.pop_back();
					stack.back() = std::max(stack.back(), b);
				}
				break;
			}
			case ExprOpType::Clamp: {
				if (stack.size() >= 3) {
					float hi = stack.back();
					stack.pop_back();
					float lo = stack.back();
					stack.pop_back();
					float x = stack.back();
					stack.back() = std::clamp(x, lo, hi);
				}
				break;
			}
			case ExprOpType::Floor: {
				if (!stack.empty())
					stack.back() = std::floor(stack.back());
				break;
			}
			case ExprOpType::Ceil: {
				if (!stack.empty())
					stack.back() = std::ceil(stack.back());
				break;
			}
			case ExprOpType::Round: {
				if (!stack.empty())
					stack.back() = std::round(stack.back());
				break;
			}
		}
	}
	return stack.empty() ? self_val : stack.back();
}

void RuleDefinition::expand_top() {
	int old_w = width();
	int old_h = height();
	min_dy--;
	int new_h = height();
	std::vector<std::vector<uint8_t>> new_when(old_w * new_h);
	std::vector<std::vector<AttributeCondition>> new_when_attrs(old_w * new_h);
	std::vector<uint8_t> new_then(old_w * new_h, 255);
	std::vector<ThenCellAction> new_then_actions(old_w * new_h);
	for (int y = 0; y < old_h; ++y) {
		for (int x = 0; x < old_w; ++x) {
			int old_i = y * old_w + x;
			int new_i = (y + 1) * old_w + x;
			if (old_i < static_cast<int>(when.size()))
				new_when[new_i] = std::move(when[old_i]);
			if (old_i < static_cast<int>(when_attrs.size()))
				new_when_attrs[new_i] = std::move(when_attrs[old_i]);
			if (old_i < static_cast<int>(then.size()))
				new_then[new_i] = then[old_i];
			if (old_i < static_cast<int>(then_actions.size()))
				new_then_actions[new_i] = then_actions[old_i];
		}
	}
	when = std::move(new_when);
	when_attrs = std::move(new_when_attrs);
	then = std::move(new_then);
	then_actions = std::move(new_then_actions);
}

void RuleDefinition::shrink_top() {
	if (min_dy >= 0 || height() <= 1)
		return;
	int old_w = width();
	min_dy++;
	int new_h = height();
	std::vector<std::vector<uint8_t>> new_when(old_w * new_h);
	std::vector<std::vector<AttributeCondition>> new_when_attrs(old_w * new_h);
	std::vector<uint8_t> new_then(old_w * new_h, 255);
	std::vector<ThenCellAction> new_then_actions(old_w * new_h);
	for (int y = 0; y < new_h; ++y) {
		for (int x = 0; x < old_w; ++x) {
			int old_i = (y + 1) * old_w + x;
			int new_i = y * old_w + x;
			if (old_i < static_cast<int>(when.size()))
				new_when[new_i] = std::move(when[old_i]);
			if (old_i < static_cast<int>(when_attrs.size()))
				new_when_attrs[new_i] = std::move(when_attrs[old_i]);
			if (old_i < static_cast<int>(then.size()))
				new_then[new_i] = then[old_i];
			if (old_i < static_cast<int>(then_actions.size()))
				new_then_actions[new_i] = then_actions[old_i];
		}
	}
	when = std::move(new_when);
	when_attrs = std::move(new_when_attrs);
	then = std::move(new_then);
	then_actions = std::move(new_then_actions);
}

void RuleDefinition::expand_bottom() {
	int old_w = width();
	max_dy++;
	int new_h = height();
	when.resize(old_w * new_h);
	when_attrs.resize(old_w * new_h);
	then.resize(old_w * new_h, 255);
	then_actions.resize(old_w * new_h);
}

void RuleDefinition::shrink_bottom() {
	if (max_dy <= 0 || height() <= 1)
		return;
	int old_w = width();
	max_dy--;
	int new_h = height();
	when.resize(old_w * new_h);
	when_attrs.resize(old_w * new_h);
	then.resize(old_w * new_h, 255);
	then_actions.resize(old_w * new_h);
}

void RuleDefinition::expand_left() {
	int old_w = width();
	int old_h = height();
	min_dx--;
	int new_w = width();
	std::vector<std::vector<uint8_t>> new_when(new_w * old_h);
	std::vector<std::vector<AttributeCondition>> new_when_attrs(new_w * old_h);
	std::vector<uint8_t> new_then(new_w * old_h, 255);
	std::vector<ThenCellAction> new_then_actions(new_w * old_h);
	for (int y = 0; y < old_h; ++y) {
		for (int x = 0; x < old_w; ++x) {
			int old_i = y * old_w + x;
			int new_i = y * new_w + (x + 1);
			if (old_i < static_cast<int>(when.size()))
				new_when[new_i] = std::move(when[old_i]);
			if (old_i < static_cast<int>(when_attrs.size()))
				new_when_attrs[new_i] = std::move(when_attrs[old_i]);
			if (old_i < static_cast<int>(then.size()))
				new_then[new_i] = then[old_i];
			if (old_i < static_cast<int>(then_actions.size()))
				new_then_actions[new_i] = then_actions[old_i];
		}
	}
	when = std::move(new_when);
	when_attrs = std::move(new_when_attrs);
	then = std::move(new_then);
	then_actions = std::move(new_then_actions);
}

void RuleDefinition::shrink_left() {
	if (min_dx >= 0 || width() <= 1)
		return;
	int old_w = width();
	int old_h = height();
	min_dx++;
	int new_w = width();
	std::vector<std::vector<uint8_t>> new_when(new_w * old_h);
	std::vector<std::vector<AttributeCondition>> new_when_attrs(new_w * old_h);
	std::vector<uint8_t> new_then(new_w * old_h, 255);
	std::vector<ThenCellAction> new_then_actions(new_w * old_h);
	for (int y = 0; y < old_h; ++y) {
		for (int x = 0; x < new_w; ++x) {
			int old_i = y * old_w + (x + 1);
			int new_i = y * new_w + x;
			if (old_i < static_cast<int>(when.size()))
				new_when[new_i] = std::move(when[old_i]);
			if (old_i < static_cast<int>(when_attrs.size()))
				new_when_attrs[new_i] = std::move(when_attrs[old_i]);
			if (old_i < static_cast<int>(then.size()))
				new_then[new_i] = then[old_i];
			if (old_i < static_cast<int>(then_actions.size()))
				new_then_actions[new_i] = then_actions[old_i];
		}
	}
	when = std::move(new_when);
	when_attrs = std::move(new_when_attrs);
	then = std::move(new_then);
	then_actions = std::move(new_then_actions);
}

void RuleDefinition::expand_right() {
	int old_w = width();
	int old_h = height();
	max_dx++;
	int new_w = width();
	std::vector<std::vector<uint8_t>> new_when(new_w * old_h);
	std::vector<std::vector<AttributeCondition>> new_when_attrs(new_w * old_h);
	std::vector<uint8_t> new_then(new_w * old_h, 255);
	std::vector<ThenCellAction> new_then_actions(new_w * old_h);
	for (int y = 0; y < old_h; ++y) {
		for (int x = 0; x < old_w; ++x) {
			int old_i = y * old_w + x;
			int new_i = y * new_w + x;
			if (old_i < static_cast<int>(when.size()))
				new_when[new_i] = std::move(when[old_i]);
			if (old_i < static_cast<int>(when_attrs.size()))
				new_when_attrs[new_i] = std::move(when_attrs[old_i]);
			if (old_i < static_cast<int>(then.size()))
				new_then[new_i] = then[old_i];
			if (old_i < static_cast<int>(then_actions.size()))
				new_then_actions[new_i] = then_actions[old_i];
		}
	}
	when = std::move(new_when);
	when_attrs = std::move(new_when_attrs);
	then = std::move(new_then);
	then_actions = std::move(new_then_actions);
}

void RuleDefinition::shrink_right() {
	if (max_dx <= 0 || width() <= 1)
		return;
	int old_w = width();
	int old_h = height();
	max_dx--;
	int new_w = width();
	std::vector<std::vector<uint8_t>> new_when(new_w * old_h);
	std::vector<std::vector<AttributeCondition>> new_when_attrs(new_w * old_h);
	std::vector<uint8_t> new_then(new_w * old_h, 255);
	std::vector<ThenCellAction> new_then_actions(new_w * old_h);
	for (int y = 0; y < old_h; ++y) {
		for (int x = 0; x < new_w; ++x) {
			int old_i = y * old_w + x;
			int new_i = y * new_w + x;
			if (old_i < static_cast<int>(when.size()))
				new_when[new_i] = std::move(when[old_i]);
			if (old_i < static_cast<int>(when_attrs.size()))
				new_when_attrs[new_i] = std::move(when_attrs[old_i]);
			if (old_i < static_cast<int>(then.size()))
				new_then[new_i] = then[old_i];
			if (old_i < static_cast<int>(then_actions.size()))
				new_then_actions[new_i] = then_actions[old_i];
		}
	}
	when = std::move(new_when);
	when_attrs = std::move(new_when_attrs);
	then = std::move(new_then);
	then_actions = std::move(new_then_actions);
}

void RuleDefinition::flip_x() {
	int new_min_dx = -static_cast<int>(max_dx);
	int new_max_dx = -static_cast<int>(min_dx);
	int new_min_dy = static_cast<int>(min_dy);
	int new_max_dy = static_cast<int>(max_dy);
	int new_w = new_max_dx - new_min_dx + 1;
	int new_h = height();
	std::vector<std::vector<uint8_t>> new_when(new_w * new_h);
	std::vector<std::vector<AttributeCondition>> new_when_attrs(new_w * new_h);
	std::vector<uint8_t> new_then(new_w * new_h, 255);
	std::vector<ThenCellAction> new_then_actions(new_w * new_h);

	for (int dy = static_cast<int>(min_dy); dy <= static_cast<int>(max_dy); ++dy) {
		for (int dx = static_cast<int>(min_dx); dx <= static_cast<int>(max_dx); ++dx) {
			int old_idx = get_cell_idx(dx, dy);
			int new_dx = -dx;
			int new_dy = dy;
			int new_idx = (new_dy - new_min_dy) * new_w + (new_dx - new_min_dx);
			if (old_idx >= 0 && old_idx < static_cast<int>(when.size()) && new_idx >= 0 && new_idx < new_w * new_h)
				new_when[new_idx] = std::move(when[old_idx]);
			if (old_idx >= 0 && old_idx < static_cast<int>(when_attrs.size()) && new_idx >= 0 &&
				new_idx < new_w * new_h)
				new_when_attrs[new_idx] = std::move(when_attrs[old_idx]);
			if (old_idx >= 0 && old_idx < static_cast<int>(then.size()) && new_idx >= 0 && new_idx < new_w * new_h)
				new_then[new_idx] = then[old_idx];
			if (old_idx >= 0 && old_idx < static_cast<int>(then_actions.size()) && new_idx >= 0 &&
				new_idx < new_w * new_h) {
				new_then_actions[new_idx] = then_actions[old_idx];
				for (size_t a = 0; a < 3; ++a) {
					if (!new_then_actions[new_idx].attr_exprs[a].empty()) {
						new_then_actions[new_idx].attr_exprs[a] =
							transform_expr_coords(new_then_actions[new_idx].attr_exprs[a], 0);
					}
				}
			}
		}
	}
	min_dx = static_cast<int8_t>(new_min_dx);
	max_dx = static_cast<int8_t>(new_max_dx);
	when = std::move(new_when);
	when_attrs = std::move(new_when_attrs);
	then = std::move(new_then);
	then_actions = std::move(new_then_actions);
}

void RuleDefinition::flip_y() {
	int new_min_dx = static_cast<int>(min_dx);
	int new_max_dx = static_cast<int>(max_dx);
	int new_min_dy = -static_cast<int>(max_dy);
	int new_max_dy = -static_cast<int>(min_dy);
	int new_w = width();
	int new_h = new_max_dy - new_min_dy + 1;
	std::vector<std::vector<uint8_t>> new_when(new_w * new_h);
	std::vector<std::vector<AttributeCondition>> new_when_attrs(new_w * new_h);
	std::vector<uint8_t> new_then(new_w * new_h, 255);
	std::vector<ThenCellAction> new_then_actions(new_w * new_h);

	for (int dy = static_cast<int>(min_dy); dy <= static_cast<int>(max_dy); ++dy) {
		for (int dx = static_cast<int>(min_dx); dx <= static_cast<int>(max_dx); ++dx) {
			int old_idx = get_cell_idx(dx, dy);
			int new_dx = dx;
			int new_dy = -dy;
			int new_idx = (new_dy - new_min_dy) * new_w + (new_dx - new_min_dx);
			if (old_idx >= 0 && old_idx < static_cast<int>(when.size()) && new_idx >= 0 && new_idx < new_w * new_h)
				new_when[new_idx] = std::move(when[old_idx]);
			if (old_idx >= 0 && old_idx < static_cast<int>(when_attrs.size()) && new_idx >= 0 &&
				new_idx < new_w * new_h)
				new_when_attrs[new_idx] = std::move(when_attrs[old_idx]);
			if (old_idx >= 0 && old_idx < static_cast<int>(then.size()) && new_idx >= 0 && new_idx < new_w * new_h)
				new_then[new_idx] = then[old_idx];
			if (old_idx >= 0 && old_idx < static_cast<int>(then_actions.size()) && new_idx >= 0 &&
				new_idx < new_w * new_h) {
				new_then_actions[new_idx] = then_actions[old_idx];
				for (size_t a = 0; a < 3; ++a) {
					if (!new_then_actions[new_idx].attr_exprs[a].empty()) {
						new_then_actions[new_idx].attr_exprs[a] =
							transform_expr_coords(new_then_actions[new_idx].attr_exprs[a], 1);
					}
				}
			}
		}
	}
	min_dy = static_cast<int8_t>(new_min_dy);
	max_dy = static_cast<int8_t>(new_max_dy);
	when = std::move(new_when);
	when_attrs = std::move(new_when_attrs);
	then = std::move(new_then);
	then_actions = std::move(new_then_actions);
}

void RuleDefinition::rotate_cw() {
	int new_min_dx = -static_cast<int>(max_dy);
	int new_max_dx = -static_cast<int>(min_dy);
	int new_min_dy = static_cast<int>(min_dx);
	int new_max_dy = static_cast<int>(max_dx);
	int new_w = new_max_dx - new_min_dx + 1;
	int new_h = new_max_dy - new_min_dy + 1;
	std::vector<std::vector<uint8_t>> new_when(new_w * new_h);
	std::vector<std::vector<AttributeCondition>> new_when_attrs(new_w * new_h);
	std::vector<uint8_t> new_then(new_w * new_h, 255);
	std::vector<ThenCellAction> new_then_actions(new_w * new_h);

	for (int dy = static_cast<int>(min_dy); dy <= static_cast<int>(max_dy); ++dy) {
		for (int dx = static_cast<int>(min_dx); dx <= static_cast<int>(max_dx); ++dx) {
			int old_idx = get_cell_idx(dx, dy);
			int new_dx = -dy;
			int new_dy = dx;
			int new_idx = (new_dy - new_min_dy) * new_w + (new_dx - new_min_dx);
			if (old_idx >= 0 && old_idx < static_cast<int>(when.size()) && new_idx >= 0 && new_idx < new_w * new_h)
				new_when[new_idx] = std::move(when[old_idx]);
			if (old_idx >= 0 && old_idx < static_cast<int>(when_attrs.size()) && new_idx >= 0 &&
				new_idx < new_w * new_h)
				new_when_attrs[new_idx] = std::move(when_attrs[old_idx]);
			if (old_idx >= 0 && old_idx < static_cast<int>(then.size()) && new_idx >= 0 && new_idx < new_w * new_h)
				new_then[new_idx] = then[old_idx];
			if (old_idx >= 0 && old_idx < static_cast<int>(then_actions.size()) && new_idx >= 0 &&
				new_idx < new_w * new_h) {
				new_then_actions[new_idx] = then_actions[old_idx];
				for (size_t a = 0; a < 3; ++a) {
					if (!new_then_actions[new_idx].attr_exprs[a].empty()) {
						new_then_actions[new_idx].attr_exprs[a] =
							transform_expr_coords(new_then_actions[new_idx].attr_exprs[a], 2);
					}
				}
			}
		}
	}
	min_dx = static_cast<int8_t>(new_min_dx);
	max_dx = static_cast<int8_t>(new_max_dx);
	min_dy = static_cast<int8_t>(new_min_dy);
	max_dy = static_cast<int8_t>(new_max_dy);
	when = std::move(new_when);
	when_attrs = std::move(new_when_attrs);
	then = std::move(new_then);
	then_actions = std::move(new_then_actions);
}

void RuleDefinition::rotate_ccw() {
	int new_min_dx = static_cast<int>(min_dy);
	int new_max_dx = static_cast<int>(max_dy);
	int new_min_dy = -static_cast<int>(max_dx);
	int new_max_dy = -static_cast<int>(min_dx);
	int new_w = new_max_dx - new_min_dx + 1;
	int new_h = new_max_dy - new_min_dy + 1;
	std::vector<std::vector<uint8_t>> new_when(new_w * new_h);
	std::vector<std::vector<AttributeCondition>> new_when_attrs(new_w * new_h);
	std::vector<uint8_t> new_then(new_w * new_h, 255);
	std::vector<ThenCellAction> new_then_actions(new_w * new_h);

	for (int dy = static_cast<int>(min_dy); dy <= static_cast<int>(max_dy); ++dy) {
		for (int dx = static_cast<int>(min_dx); dx <= static_cast<int>(max_dx); ++dx) {
			int old_idx = get_cell_idx(dx, dy);
			int new_dx = dy;
			int new_dy = -dx;
			int new_idx = (new_dy - new_min_dy) * new_w + (new_dx - new_min_dx);
			if (old_idx >= 0 && old_idx < static_cast<int>(when.size()) && new_idx >= 0 && new_idx < new_w * new_h)
				new_when[new_idx] = std::move(when[old_idx]);
			if (old_idx >= 0 && old_idx < static_cast<int>(when_attrs.size()) && new_idx >= 0 &&
				new_idx < new_w * new_h)
				new_when_attrs[new_idx] = std::move(when_attrs[old_idx]);
			if (old_idx >= 0 && old_idx < static_cast<int>(then.size()) && new_idx >= 0 && new_idx < new_w * new_h)
				new_then[new_idx] = then[old_idx];
			if (old_idx >= 0 && old_idx < static_cast<int>(then_actions.size()) && new_idx >= 0 &&
				new_idx < new_w * new_h) {
				new_then_actions[new_idx] = then_actions[old_idx];
				for (size_t a = 0; a < 3; ++a) {
					if (!new_then_actions[new_idx].attr_exprs[a].empty()) {
						new_then_actions[new_idx].attr_exprs[a] =
							transform_expr_coords(new_then_actions[new_idx].attr_exprs[a], 3);
					}
				}
			}
		}
	}
	min_dx = static_cast<int8_t>(new_min_dx);
	max_dx = static_cast<int8_t>(new_max_dx);
	min_dy = static_cast<int8_t>(new_min_dy);
	max_dy = static_cast<int8_t>(new_max_dy);
	when = std::move(new_when);
	when_attrs = std::move(new_when_attrs);
	then = std::move(new_then);
	then_actions = std::move(new_then_actions);
}

void RuleDefinition::reset_to_3x3(uint8_t center_id) {
	std::vector<std::vector<uint8_t>> new_when(9, std::vector<uint8_t>{});
	std::vector<std::vector<AttributeCondition>> new_when_attrs(9, std::vector<AttributeCondition>{});
	std::vector<uint8_t> new_then(9, 255);
	std::vector<ThenCellAction> new_then_actions(9, ThenCellAction{});
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			int old_idx = get_cell_idx(dx, dy);
			int new_idx = (dy + 1) * 3 + (dx + 1);
			if (old_idx >= 0 && old_idx < static_cast<int>(when.size())) {
				new_when[new_idx] = std::move(when[old_idx]);
				if (old_idx < static_cast<int>(when_attrs.size()))
					new_when_attrs[new_idx] = std::move(when_attrs[old_idx]);
				new_then[new_idx] = then[old_idx];
				if (old_idx < static_cast<int>(then_actions.size()))
					new_then_actions[new_idx] = then_actions[old_idx];
			}
		}
	}
	min_dx = -1;
	max_dx = 1;
	min_dy = -1;
	max_dy = 1;
	when = std::move(new_when);
	when_attrs = std::move(new_when_attrs);
	then = std::move(new_then);
	then_actions = std::move(new_then_actions);
	if (when[4].empty()) {
		when[4] = {center_id};
	}
}

void MaterialDefinition::sync_rule_order() {
	std::vector<RuleReference> valid_refs;

	const MaterialDefinition* parent = nullptr;
	if (inherits_from != 255) {
		const auto& p = MaterialManager::get_material(inherits_from);
		if (p.id == inherits_from) {
			parent = &p;
		}
	}

	size_t parent_rule_count = parent ? parent->rules.size() : 0;
	size_t custom_rule_count = rules.size();

	for (const auto& reference : rule_order) {
		if (reference.is_inherited) {
			if (parent && reference.index < parent_rule_count &&
				std::find(valid_refs.begin(), valid_refs.end(), reference) == valid_refs.end()) {
				valid_refs.push_back(reference);
			}
		} else {
			if (reference.index < custom_rule_count &&
				std::find(valid_refs.begin(), valid_refs.end(), reference) == valid_refs.end()) {
				valid_refs.push_back(reference);
			}
		}
	}

	if (parent && parent_rule_count > last_synced_parent_rule_count) {
		for (size_t i = last_synced_parent_rule_count; i < parent_rule_count; ++i) {
			RuleReference reference{true, i};
			if (std::find(valid_refs.begin(), valid_refs.end(), reference) == valid_refs.end()) {
				if (i <= valid_refs.size()) {
					valid_refs.insert(valid_refs.begin() + static_cast<std::ptrdiff_t>(i), reference);
				} else {
					valid_refs.push_back(reference);
				}
			}
		}
		last_synced_parent_rule_count = parent_rule_count;
	} else if (!parent) {
		last_synced_parent_rule_count = 0;
	}

	for (size_t i = 0; i < custom_rule_count; ++i) {
		RuleReference reference{false, i};
		if (std::find(valid_refs.begin(), valid_refs.end(), reference) == valid_refs.end()) {
			valid_refs.push_back(reference);
		}
	}

	rule_order = valid_refs;
}

RuleDefinition MaterialDefinition::get_effective_rule(size_t order_idx) const {
	if (order_idx >= rule_order.size()) {
		RuleDefinition dummy;
		dummy.init_default(id);
		return dummy;
	}

	const RuleReference& reference = rule_order[order_idx];
	if (reference.is_inherited && inherits_from != 255) {
		const auto& parent = MaterialManager::get_material(inherits_from);
		if (parent.id == inherits_from && reference.index < parent.rules.size()) {
			RuleDefinition r = parent.rules[reference.index];
			MaterialManager::replace_self_references(r, parent.id, id);
			int c_idx = r.get_center_idx();
			if (c_idx >= 0 && c_idx < static_cast<int>(r.when.size())) {
				r.when[c_idx] = {id};
			}
			return r;
		}
	} else if (!reference.is_inherited) {
		if (reference.index < rules.size()) {
			RuleDefinition r = rules[reference.index];
			int c_idx = r.get_center_idx();
			if (c_idx >= 0 && c_idx < static_cast<int>(r.when.size())) {
				r.when[c_idx] = {id};
			}
			return r;
		}
	}

	RuleDefinition dummy;
	dummy.init_default(id);
	return dummy;
}

void MaterialManager::replace_self_references(RuleDefinition& rule, uint8_t old_id, uint8_t new_id) {
	for (auto& when_cell : rule.when) {
		for (auto& id : when_cell) {
			if (id == old_id) {
				id = new_id;
			}
		}
	}
	for (auto& then_cell : rule.then) {
		if (then_cell == old_id) {
			then_cell = new_id;
		}
	}
}

void MaterialManager::replace_self_references_in_material(MaterialDefinition& mat, uint8_t old_id, uint8_t new_id) {
	for (auto& r : mat.rules) {
		replace_self_references(r, old_id, new_id);
	}
}

void MaterialManager::sync_inherited_rules(uint8_t id) {
	uint8_t idx = material_by_id[id];
	if (idx < materials.size() && materials[idx].id == id) {
		materials[idx].sync_rule_order();
	}
}

static MaterialDefinition parse_material_from_json(const nlohmann::ordered_json& j, const std::string& default_name,
												   uint8_t fallback_id) {
	MaterialDefinition mat;
	mat.name = j.value("name", default_name);
	mat.id = j.value("id", fallback_id);
	mat.inherits_from = j.value("inherits_from", 255);

	if (j.contains("color")) {
		mat.color = j["color"].get<std::array<uint8_t, 3>>();
	} else {
		mat.color = {64, 64, 64};
	}

	if (j.contains("default_attributes") && j["default_attributes"].is_array()) {
		const auto& arr = j["default_attributes"];
		for (size_t i = 0; i < std::min(arr.size(), size_t(3)); ++i) {
			mat.default_attributes[i] = arr[i].get<double>();
		}
	}

	if (j.contains("color_modifiers") && j["color_modifiers"].is_array()) {
		const auto& cm_arr = j["color_modifiers"];
		for (size_t i = 0; i < std::min(cm_arr.size(), size_t(3)); ++i) {
			const auto& c_j = cm_arr[i];
			if (c_j.is_object()) {
				mat.attr_color_modifiers[i].enabled = c_j.value("enabled", false);
				if (c_j.contains("gradient") && c_j["gradient"].is_array()) {
					mat.attr_color_modifiers[i].gradient.stops.clear();
					for (const auto& s_j : c_j["gradient"]) {
						if (s_j.is_object()) {
							GradientStop s;
							s.pos = s_j.value("pos", 0.0f);
							if (s_j.contains("color") && s_j["color"].is_array()) {
								for (size_t c = 0; c < std::min(s_j["color"].size(), size_t(4)); ++c) {
									s.color[c] = s_j["color"][c].get<float>();
								}
							}
							mat.attr_color_modifiers[i].gradient.stops.push_back(s);
						}
					}
					mat.attr_color_modifiers[i].gradient.sort_stops();
				}
			}
		}
	}

	if (j.contains("rules")) {
		for (const auto& r_j : j["rules"]) {
			if (r_j.contains("inherited_rule")) {
				size_t parent_idx = r_j["inherited_rule"].get<size_t>();
				mat.rule_order.push_back({true, parent_idx});
			} else {
				RuleDefinition r;
				int8_t min_dx = -1, max_dx = 1, min_dy = -1, max_dy = 1;
				if (r_j.contains("bounds") && r_j["bounds"].is_array() && r_j["bounds"].size() >= 4) {
					min_dx = r_j["bounds"][0].get<int8_t>();
					max_dx = r_j["bounds"][1].get<int8_t>();
					min_dy = r_j["bounds"][2].get<int8_t>();
					max_dy = r_j["bounds"][3].get<int8_t>();
				} else if (r_j.contains("when") && r_j["when"].is_array() && r_j["when"].size() == 25) {
					min_dx = -2;
					max_dx = 2;
					min_dy = -2;
					max_dy = 2;
				}

				r.min_dx = min_dx;
				r.max_dx = max_dx;
				r.min_dy = min_dy;
				r.max_dy = max_dy;
				int total = r.total_cells();
				r.when.assign(total, std::vector<uint8_t>{});
				r.when_attrs.assign(total, std::vector<AttributeCondition>{});
				r.then.assign(total, 255);
				r.then_actions.assign(total, ThenCellAction{});

				if (r_j.contains("when") && r_j["when"].is_array()) {
					const auto& when_arr = r_j["when"];
					if (when_arr.size() == 25 && total != 25) {
						for (int dy = static_cast<int>(min_dy); dy <= static_cast<int>(max_dy); ++dy) {
							for (int dx = static_cast<int>(min_dx); dx <= static_cast<int>(max_dx); ++dx) {
								if (dx >= -2 && dx <= 2 && dy >= -2 && dy <= 2) {
									int old_idx = (dy + 2) * 5 + (dx + 2);
									int new_idx = r.get_cell_idx(dx, dy);
									if (new_idx >= 0 && new_idx < static_cast<int>(r.when.size())) {
										r.when[new_idx] = when_arr[old_idx].get<std::vector<uint8_t>>();
									}
								}
							}
						}
					} else {
						size_t count = std::min(when_arr.size(), static_cast<size_t>(total));
						for (size_t i = 0; i < count; ++i) {
							r.when[i] = when_arr[i].get<std::vector<uint8_t>>();
						}
					}
				}

				if (r_j.contains("when_attrs") && r_j["when_attrs"].is_array()) {
					const auto& wa_arr = r_j["when_attrs"];
					size_t count = std::min(wa_arr.size(), static_cast<size_t>(total));
					for (size_t i = 0; i < count; ++i) {
						if (wa_arr[i].is_array()) {
							for (const auto& cond_j : wa_arr[i]) {
								AttributeCondition ac;
								ac.attr_index = cond_j.value("attr", uint8_t(0));
								ac.op = static_cast<CompareOp>(cond_j.value("op", uint8_t(0)));
								ac.target = static_cast<CompareTarget>(cond_j.value("target", uint8_t(0)));
								ac.const_value = cond_j.value("val", 0.0);
								r.when_attrs[i].push_back(ac);
							}
						}
					}
				}

				if (r_j.contains("then") && r_j["then"].is_array()) {
					const auto& then_arr = r_j["then"];
					if (then_arr.size() == 25 && total != 25) {
						for (int dy = static_cast<int>(min_dy); dy <= static_cast<int>(max_dy); ++dy) {
							for (int dx = static_cast<int>(min_dx); dx <= static_cast<int>(max_dx); ++dx) {
								if (dx >= -2 && dx <= 2 && dy >= -2 && dy <= 2) {
									int old_idx = (dy + 2) * 5 + (dx + 2);
									int new_idx = r.get_cell_idx(dx, dy);
									if (new_idx >= 0 && new_idx < static_cast<int>(r.then.size())) {
										r.then[new_idx] = then_arr[old_idx].get<uint8_t>();
									}
								}
							}
						}
					} else {
						size_t count = std::min(then_arr.size(), static_cast<size_t>(total));
						for (size_t i = 0; i < count; ++i) {
							r.then[i] = then_arr[i].get<uint8_t>();
						}
					}
				}

				if (r_j.contains("then_actions") && r_j["then_actions"].is_array()) {
					const auto& ta_arr = r_j["then_actions"];
					size_t count = std::min(ta_arr.size(), static_cast<size_t>(total));
					for (size_t i = 0; i < count; ++i) {
						const auto& act_j = ta_arr[i];
						if (act_j.is_object()) {
							if (act_j.contains("exprs") && act_j["exprs"].is_array()) {
								for (size_t a = 0; a < std::min(act_j["exprs"].size(), size_t(3)); ++a) {
									r.then_actions[i].attr_exprs[a] = act_j["exprs"][a].get<std::string>();
								}
							}
							if (act_j.contains("updated")) {
								r.then_actions[i].updated = act_j["updated"].get<bool>();
							}
						}
					}
				}

				r.symmetry.flip_x = r_j.contains("sym_x") && r_j["sym_x"].get<bool>();
				r.symmetry.flip_y = r_j.contains("sym_y") && r_j["sym_y"].get<bool>();
				r.symmetry.rotate = r_j.contains("sym_rot") && r_j["sym_rot"].get<bool>();
				r.enabled = !r_j.contains("enabled") || r_j["enabled"].get<bool>();
				r.multi_apply = r_j.contains("multi_apply") && r_j["multi_apply"].get<bool>();
				r.continue_to_next = r_j.contains("continue_to_next") && r_j["continue_to_next"].get<bool>();
				r.chance = r_j.contains("chance") ? r_j["chance"].get<float>() : 100.0f;
				int c_idx = r.get_center_idx();
				if (c_idx >= 0 && c_idx < static_cast<int>(r.when.size())) {
					r.when[c_idx] = {mat.id};
				}

				mat.rules.push_back(r);
				mat.rule_order.push_back({false, mat.rules.size() - 1});
			}
		}
	}

	mat.last_synced_parent_rule_count = j.value("last_synced_parent_rule_count", (size_t)0);

	return mat;
}

static bool is_empty_material_default(const MaterialDefinition& m) {
	if (m.id != 0)
		return false;
	if (m.color[0] != 64 || m.color[1] != 64 || m.color[2] != 64)
		return false;
	if (!m.rules.empty() || !m.rule_order.empty())
		return false;
	if (m.inherits_from != 255)
		return false;
	for (int a = 0; a < 3; ++a) {
		if (m.default_attributes[a] != 0.0)
			return false;
		if (m.attr_color_modifiers[a].enabled)
			return false;
	}
	return true;
}

void MaterialManager::load_all_materials(std::string_view directory_path) {
	materials.clear();

	MaterialDefinition empty_mat = default_empty;
	empty_mat.id = 0;

	std::vector<MaterialDefinition> loaded_mats;

	if (fs::exists(directory_path.data()) && fs::is_directory(directory_path.data())) {
		for (const auto& entry : fs::directory_iterator(directory_path.data())) {
			if (entry.is_regular_file() && entry.path().extension().string() == ".mat") {
				std::ifstream file(entry.path());
				if (!file.is_open())
					continue;

				try {
					nlohmann::ordered_json j = nlohmann::ordered_json::parse(file);
					MaterialDefinition mat = parse_material_from_json(j, entry.path().stem().string(), 255);

					if (mat.id == 0 || entry.path().stem().string() == "empty") {
						mat.id = 0;
						mat.name = "empty";
						empty_mat = mat;
					} else {
						loaded_mats.push_back(mat);
					}
				} catch (...) {}
			}
		}
	}

	materials.push_back(empty_mat);

	std::sort(loaded_mats.begin(), loaded_mats.end(),
			  [](const MaterialDefinition& a, const MaterialDefinition& b) { return a.id < b.id; });

	std::array<bool, 256> id_taken{};
	id_taken[0] = true;

	for (auto& mat : loaded_mats) {
		if (mat.id == 0 || mat.id == 255 || id_taken[mat.id]) {
			mat.id = get_unused_id();
		}
		id_taken[mat.id] = true;
		for (auto& r : mat.rules) {
			int c_idx = r.get_center_idx();
			if (c_idx >= 0 && c_idx < static_cast<int>(r.when.size())) {
				r.when[c_idx] = {mat.id};
			}
		}

		materials.push_back(mat);
	}

	rebuild_compiled_rules();
}

void MaterialManager::save_all_materials(std::string_view directory_path) {
	fs::create_directories(directory_path.data());

	for (const auto& entry : fs::directory_iterator(directory_path.data())) {
		if (entry.is_regular_file() && entry.path().extension().string() == ".mat") {
			std::string stem = entry.path().stem().string();
			bool found = false;
			for (const auto& m : materials) {
				if (m.name == stem) {
					if (is_empty_material_default(m)) {
						break;
					}
					found = true;
					break;
				}
			}
			if (!found) {
				fs::remove(entry.path());
			}
		}
	}

	for (const auto& mat : materials) {
		if (is_empty_material_default(mat)) {
			continue;
		}

		nlohmann::ordered_json j;
		j["name"] = mat.name;
		j["id"] = mat.id;
		j["color"] = mat.color;
		bool has_non_zero_defaults = false;
		for (int a = 0; a < 3; ++a) {
			if (mat.default_attributes[a] != 0) {
				has_non_zero_defaults = true;
				break;
			}
		}
		if (has_non_zero_defaults) {
			j["default_attributes"] = mat.default_attributes;
		}
		bool has_color_modifiers = false;
		for (const auto& cm : mat.attr_color_modifiers) {
			if (cm.enabled) {
				has_color_modifiers = true;
				break;
			}
		}
		if (has_color_modifiers) {
			nlohmann::ordered_json cm_arr = nlohmann::ordered_json::array();
			for (const auto& cm : mat.attr_color_modifiers) {
				nlohmann::ordered_json cm_j;
				cm_j["enabled"] = cm.enabled;
				nlohmann::ordered_json grad_arr = nlohmann::ordered_json::array();
				for (const auto& stop : cm.gradient.stops) {
					nlohmann::ordered_json stop_j;
					stop_j["pos"] = stop.pos;
					stop_j["color"] = stop.color;
					grad_arr.push_back(stop_j);
				}
				cm_j["gradient"] = grad_arr;
				cm_arr.push_back(cm_j);
			}
			j["color_modifiers"] = cm_arr;
		}
		if (mat.inherits_from != 255) {
			j["inherits_from"] = mat.inherits_from;
			j["last_synced_parent_rule_count"] = mat.last_synced_parent_rule_count;
		}
		nlohmann::ordered_json rules_arr = nlohmann::ordered_json::array();
		for (const auto& reference : mat.rule_order) {
			if (reference.is_inherited) {
				nlohmann::ordered_json r;
				r["inherited_rule"] = reference.index;
				rules_arr.push_back(r);
			} else {
				if (reference.index < mat.rules.size()) {
					const auto& rule = mat.rules[reference.index];
					nlohmann::ordered_json r;
					auto when_copy = rule.when;
					int c_idx = rule.get_center_idx();
					if (c_idx >= 0 && c_idx < static_cast<int>(when_copy.size())) {
						when_copy[c_idx].clear();
					}
					r["when"] = when_copy;

					bool has_when_attrs = false;
					for (const auto& cell_conds : rule.when_attrs) {
						if (!cell_conds.empty()) {
							has_when_attrs = true;
							break;
						}
					}
					if (has_when_attrs) {
						nlohmann::ordered_json wa_arr = nlohmann::ordered_json::array();
						for (const auto& cell_conds : rule.when_attrs) {
							nlohmann::ordered_json cell_arr = nlohmann::ordered_json::array();
							for (const auto& ac : cell_conds) {
								nlohmann::ordered_json cond_j;
								cond_j["attr"] = ac.attr_index;
								cond_j["op"] = static_cast<uint8_t>(ac.op);
								cond_j["target"] = static_cast<uint8_t>(ac.target);
								cond_j["val"] = ac.const_value;
								cell_arr.push_back(cond_j);
							}
							wa_arr.push_back(cell_arr);
						}
						r["when_attrs"] = wa_arr;
					}

					r["then"] = rule.then;

					bool has_custom_then_actions = false;
					for (const auto& act : rule.then_actions) {
						if (!act.attr_exprs[0].empty() || !act.attr_exprs[1].empty() || !act.attr_exprs[2].empty() ||
							!act.updated) {
							has_custom_then_actions = true;
							break;
						}
					}
					if (has_custom_then_actions) {
						nlohmann::ordered_json ta_arr = nlohmann::ordered_json::array();
						for (const auto& act : rule.then_actions) {
							nlohmann::ordered_json act_j;
							if (!act.attr_exprs[0].empty() || !act.attr_exprs[1].empty() ||
								!act.attr_exprs[2].empty()) {
								act_j["exprs"] = act.attr_exprs;
							}
							if (!act.updated) {
								act_j["updated"] = false;
							}
							ta_arr.push_back(act_j);
						}
						r["then_actions"] = ta_arr;
					}

					r["bounds"] = {rule.min_dx, rule.max_dx, rule.min_dy, rule.max_dy};
					if (rule.symmetry.flip_x)
						r["sym_x"] = rule.symmetry.flip_x;
					if (rule.symmetry.flip_y)
						r["sym_y"] = rule.symmetry.flip_y;
					if (rule.symmetry.rotate)
						r["sym_rot"] = rule.symmetry.rotate;
					if (!rule.enabled)
						r["enabled"] = false;
					if (rule.multi_apply)
						r["multi_apply"] = true;
					if (rule.continue_to_next)
						r["continue_to_next"] = true;
					if (rule.chance != 100.0f)
						r["chance"] = rule.chance;
					rules_arr.push_back(r);
				}
			}
		}
		j["rules"] = rules_arr;

		std::string filename = std::string(directory_path) + "/" + mat.name + ".mat";
		std::ofstream file(filename);
		if (file.is_open()) {
			file << j.dump(4);
		}
	}
}

uint8_t MaterialManager::add_material(const MaterialDefinition& mat) {
	MaterialDefinition m = mat;
	uint8_t old_id = mat.id;
	m.id = get_unused_id();
	replace_self_references_in_material(m, old_id, m.id);
	for (auto& r : m.rules) {
		int c_idx = r.get_center_idx();
		if (c_idx >= 0 && c_idx < static_cast<int>(r.when.size())) {
			r.when[c_idx] = {m.id};
		}
	}
	materials.push_back(m);
	rebuild_compiled_rules();
	return m.id;
}

void MaterialManager::update_material_name(uint8_t id, std::string_view name) {
	uint8_t idx = material_by_id[id];
	if (idx < materials.size() && materials[idx].id == id) {
		materials[idx].name = name;
	}
}

void MaterialManager::update_material_rules(uint8_t id, const MaterialDefinition& mat) {
	uint8_t idx = material_by_id[id];
	if (idx < materials.size() && materials[idx].id == id) {
		materials[idx].rules = mat.rules;
		materials[idx].rule_order = mat.rule_order;
		materials[idx].last_synced_parent_rule_count = mat.last_synced_parent_rule_count;
		rebuild_compiled_rules();
	}
}

void MaterialManager::set_material_inheritance(uint8_t id, uint8_t parent_id) {
	uint8_t idx = material_by_id[id];
	if (idx >= materials.size() || materials[idx].id != id)
		return;

	if (parent_id != 255) {
		uint8_t curr = parent_id;
		while (curr != 255) {
			if (curr == id) {
				return;
			}
			const auto& p = get_material(curr);
			if (p.id != curr)
				break;
			curr = p.inherits_from;
		}
	}

	auto& mat = materials[idx];
	mat.inherits_from = parent_id;

	std::erase_if(mat.rule_order, [](const RuleReference& r) { return r.is_inherited; });
	mat.last_synced_parent_rule_count = 0;

	if (parent_id != 255) {
		const auto& parent = get_material(parent_id);
		if (parent.id == parent_id) {
			std::vector<RuleReference> new_order;
			new_order.reserve(parent.rules.size() + mat.rule_order.size());
			for (size_t i = 0; i < parent.rules.size(); ++i) {
				new_order.push_back({true, i});
			}
			for (const auto& reference : mat.rule_order) {
				if (!reference.is_inherited) {
					new_order.push_back(reference);
				}
			}
			mat.rule_order = new_order;
			mat.last_synced_parent_rule_count = parent.rules.size();
		}
	}

	mat.sync_rule_order();
	rebuild_compiled_rules();
}

void MaterialManager::update_material_color(uint8_t id, const MaterialDefinition& mat) {
	uint8_t idx = material_by_id[id];
	if (idx >= materials.size() || materials[idx].id != id)
		return;
	materials[idx].color = mat.color;
	runtime_materials[id].packed_color = pack_color(mat.color);
	runtime_materials[id].base_color = mat.color;

	if (Vulkan::is_available()) {
		Vulkan::update_rules();
	}

	Grid::draw_material(id);
}

void MaterialManager::remove_material(uint8_t id) {
	uint8_t idx = material_by_id[id];
	if (idx >= materials.size() || materials[idx].id != id)
		return;

	std::vector<uint8_t> old_to_new(256);
	for (int i = 0; i < 256; ++i) {
		old_to_new[i] = static_cast<uint8_t>(i);
	}
	old_to_new[id] = 0;

	Grid::remap_materials(old_to_new);
	Grid::draw_material(0);

	materials.erase(materials.begin() + idx);

	for (auto& m : materials) {
		if (m.inherits_from == id) {
			m.inherits_from = 255;
			std::erase_if(m.rule_order, [](const RuleReference& r) { return r.is_inherited; });
			m.last_synced_parent_rule_count = 0;
		}
		for (auto& r : m.rules) {
			for (auto& when_cell : r.when) {
				std::erase(when_cell, id);
			}
			for (auto& then_cell : r.then) {
				if (then_cell == id) {
					then_cell = 255;
				}
			}
		}
	}

	rebuild_compiled_rules();
}

void MaterialManager::rebuild_compiled_rules() {
	material_by_id.fill(255);
	for (size_t i = 0; i < materials.size(); ++i) {
		material_by_id[materials[i].id] = static_cast<uint8_t>(i);
	}

	for (size_t i = 0; i < materials.size(); ++i) {
		materials[i].sync_rule_order();
	}

	runtime_materials.fill(default_runtime_empty);

	int max_rx = 1;
	int max_ry = 1;
	const int gw = static_cast<int>(Grid::get_width());

	auto transform_variant = [gw](const CompiledRuleVariant& src, auto coord_func) {
		CompiledRuleVariant dst;
		dst.min_dx = 0;
		dst.max_dx = 0;
		dst.min_dy = 0;
		dst.max_dy = 0;
		dst.has_attribute_logic = src.has_attribute_logic;
		for (const auto& cond : src.conditions) {
			auto [ndx, ndy] = coord_func(cond.dx, cond.dy);
			dst.min_dx = std::min(dst.min_dx, static_cast<int8_t>(ndx));
			dst.max_dx = std::max(dst.max_dx, static_cast<int8_t>(ndx));
			dst.min_dy = std::min(dst.min_dy, static_cast<int8_t>(ndy));
			dst.max_dy = std::max(dst.max_dy, static_cast<int8_t>(ndy));
			dst.conditions.push_back({ndy * gw + ndx, static_cast<int8_t>(ndx), static_cast<int8_t>(ndy), cond.allowed,
									  cond.attr_conditions});
		}
		for (const auto& act : src.actions) {
			auto [ndx, ndy] = coord_func(act.dx, act.dy);
			dst.min_dx = std::min(dst.min_dx, static_cast<int8_t>(ndx));
			dst.max_dx = std::max(dst.max_dx, static_cast<int8_t>(ndx));
			dst.min_dy = std::min(dst.min_dy, static_cast<int8_t>(ndy));
			dst.max_dy = std::max(dst.max_dy, static_cast<int8_t>(ndy));
			OutputAction dst_act;
			dst_act.offset = ndy * gw + ndx;
			dst_act.dx = static_cast<int8_t>(ndx);
			dst_act.dy = static_cast<int8_t>(ndy);
			dst_act.target_material = act.target_material;
			dst_act.updated = act.updated;
			for (size_t a = 0; a < 3; ++a) {
				dst_act.attr_expr_ops[a] = act.attr_expr_ops[a];
				for (auto& op : dst_act.attr_expr_ops[a]) {
					if (op.type == ExprOpType::LoadCellAttr) {
						auto [ndx_op, ndy_op] = coord_func(op.dx, op.dy);
						op.dx = static_cast<int8_t>(ndx_op);
						op.dy = static_cast<int8_t>(ndy_op);
					}
				}
			}
			dst.actions.push_back(dst_act);
		}
		std::sort(dst.conditions.begin(), dst.conditions.end(), [](const ConditionCheck& a, const ConditionCheck& b) {
			return std::tie(a.dy, a.dx) < std::tie(b.dy, b.dx);
		});
		std::sort(dst.actions.begin(), dst.actions.end(), [](const OutputAction& a, const OutputAction& b) {
			return std::tie(a.dy, a.dx) < std::tie(b.dy, b.dx);
		});
		return dst;
	};

	auto are_variants_equal = [](const CompiledRuleVariant& a, const CompiledRuleVariant& b) {
		if (a.conditions.size() != b.conditions.size() || a.actions.size() != b.actions.size())
			return false;
		for (size_t i = 0; i < a.conditions.size(); ++i) {
			if (a.conditions[i].dx != b.conditions[i].dx || a.conditions[i].dy != b.conditions[i].dy ||
				a.conditions[i].allowed != b.conditions[i].allowed ||
				a.conditions[i].attr_conditions != b.conditions[i].attr_conditions)
				return false;
		}
		for (size_t i = 0; i < a.actions.size(); ++i) {
			if (a.actions[i].dx != b.actions[i].dx || a.actions[i].dy != b.actions[i].dy ||
				a.actions[i].target_material != b.actions[i].target_material ||
				a.actions[i].updated != b.actions[i].updated ||
				a.actions[i].attr_expr_ops != b.actions[i].attr_expr_ops)
				return false;
		}
		return true;
	};

	for (const auto& m : materials) {
		RuntimeMaterial rm;
		rm.id = m.id;
		rm.packed_color = pack_color(m.color);
		rm.base_color = m.color;
		rm.has_color_modifiers = false;
		rm.color_modifiers.clear();
		const auto& meta = SetManager::get_current_metadata();
		for (size_t a = 0; a < 3; ++a) {
			if (m.attr_color_modifiers[a].enabled) {
				RuntimeColorMod rcm;
				rcm.attr_index = static_cast<uint8_t>(a);
				rcm.attr_min = (a < meta.attributes.size()) ? static_cast<float>(meta.attributes[a].min_val) : 0.0f;
				rcm.attr_max = (a < meta.attributes.size()) ? static_cast<float>(meta.attributes[a].max_val) : 1.0f;
				rcm.gradient = m.attr_color_modifiers[a].gradient;
				rcm.gradient.sort_stops();
				rm.color_modifiers.push_back(rcm);
				rm.has_color_modifiers = true;
			}
		}

		for (size_t r_idx = 0; r_idx < m.rule_order.size(); ++r_idx) {
			RuleDefinition ur = m.get_effective_rule(r_idx);
			if (!ur.enabled) {
				continue;
			}
			CompiledRule cur;
			cur.chance = ur.chance;
			cur.multi_apply = ur.multi_apply;
			cur.continue_to_next = ur.continue_to_next;

			CompiledRuleVariant base_rule;
			base_rule.min_dx = ur.min_dx;
			base_rule.max_dx = ur.max_dx;
			base_rule.min_dy = ur.min_dy;
			base_rule.max_dy = ur.max_dy;

			for (int dy = static_cast<int>(ur.min_dy); dy <= static_cast<int>(ur.max_dy); ++dy) {
				for (int dx = static_cast<int>(ur.min_dx); dx <= static_cast<int>(ur.max_dx); ++dx) {
					int idx = ur.get_cell_idx(dx, dy);
					if (idx < 0 || idx >= static_cast<int>(ur.when.size()))
						continue;
					const auto& w_list = ur.when[idx];
					uint8_t then_val = (idx < static_cast<int>(ur.then.size())) ? ur.then[idx] : 255;
					const std::vector<AttributeCondition>& wa = (idx < static_cast<int>(ur.when_attrs.size()))
																	? ur.when_attrs[idx]
																	: std::vector<AttributeCondition>{};
					const ThenCellAction& ta =
						(idx < static_cast<int>(ur.then_actions.size())) ? ur.then_actions[idx] : ThenCellAction{};

					if (!wa.empty()) {
						base_rule.has_attribute_logic = true;
					}
					bool has_attr_act =
						(!ta.attr_exprs[0].empty() || !ta.attr_exprs[1].empty() || !ta.attr_exprs[2].empty());
					if (has_attr_act) {
						base_rule.has_attribute_logic = true;
					}

					if (dx == 0 && dy == 0) {
						if (!wa.empty()) {
							ConditionCheck cond;
							cond.offset = 0;
							cond.dx = 0;
							cond.dy = 0;
							cond.allowed.set(m.id);
							cond.attr_conditions = wa;
							base_rule.conditions.push_back(cond);
						}
						if (then_val != 255 || has_attr_act || ta.updated) {
							OutputAction act;
							act.offset = 0;
							act.dx = 0;
							act.dy = 0;
							act.target_material = then_val;
							act.updated = ta.updated;
							for (size_t a = 0; a < 3; ++a) {
								if (!ta.attr_exprs[a].empty()) {
									std::vector<ExprOp> ops;
									if (parse_and_compile_expr(ta.attr_exprs[a], static_cast<uint8_t>(a), ops, nullptr,
															   0, 0)) {
										act.attr_expr_ops[a] = std::move(ops);
									}
								}
							}
							base_rule.actions.push_back(act);
						}
						continue;
					}

					if (!w_list.empty() || !wa.empty()) {
						ConditionCheck cond;
						cond.offset = dy * gw + dx;
						cond.dx = static_cast<int8_t>(dx);
						cond.dy = static_cast<int8_t>(dy);
						if (w_list.empty()) {
							cond.allowed.set();
						} else {
							cond.allowed.reset();
							for (uint8_t wid : w_list) {
								cond.allowed.set(wid);
							}
						}
						cond.attr_conditions = wa;
						base_rule.conditions.push_back(cond);
					}

					if (then_val != 255 || has_attr_act) {
						OutputAction act;
						act.offset = dy * gw + dx;
						act.dx = static_cast<int8_t>(dx);
						act.dy = static_cast<int8_t>(dy);
						act.target_material = then_val;
						act.updated = ta.updated;
						for (size_t a = 0; a < 3; ++a) {
							if (!ta.attr_exprs[a].empty()) {
								std::vector<ExprOp> ops;
								if (parse_and_compile_expr(ta.attr_exprs[a], static_cast<uint8_t>(a), ops, nullptr, dx,
														   dy)) {
									act.attr_expr_ops[a] = std::move(ops);
								}
							}
						}
						base_rule.actions.push_back(act);
					}
				}
			}

			std::sort(base_rule.conditions.begin(), base_rule.conditions.end(),
					  [](const ConditionCheck& a, const ConditionCheck& b) {
						  return std::tie(a.dy, a.dx) < std::tie(b.dy, b.dx);
					  });
			std::sort(base_rule.actions.begin(), base_rule.actions.end(),
					  [](const OutputAction& a, const OutputAction& b) {
						  return std::tie(a.dy, a.dx) < std::tie(b.dy, b.dx);
					  });

			cur.variants.push_back(base_rule);

			auto add_unique_variant = [&cur, &are_variants_equal](const CompiledRuleVariant& candidate) {
				for (const auto& existing : cur.variants) {
					if (are_variants_equal(existing, candidate)) {
						return;
					}
				}
				cur.variants.push_back(candidate);
			};

			if (ur.symmetry.rotate) {
				CompiledRuleVariant r_90 =
					transform_variant(base_rule, [](int dx, int dy) { return std::pair(-dy, dx); });
				add_unique_variant(r_90);

				CompiledRuleVariant r_180 = transform_variant(r_90, [](int dx, int dy) { return std::pair(-dy, dx); });
				add_unique_variant(r_180);

				CompiledRuleVariant r_270 = transform_variant(r_180, [](int dx, int dy) { return std::pair(-dy, dx); });
				add_unique_variant(r_270);
			}

			if (ur.symmetry.flip_x || ur.symmetry.flip_y) {
				std::vector<CompiledRuleVariant> current_variants = cur.variants;
				for (const auto& src : current_variants) {
					if (ur.symmetry.flip_x) {
						add_unique_variant(transform_variant(src, [](int dx, int dy) { return std::pair(-dx, dy); }));
					}
					if (ur.symmetry.flip_y) {
						add_unique_variant(transform_variant(src, [](int dx, int dy) { return std::pair(dx, -dy); }));
					}
					if (ur.symmetry.flip_x && ur.symmetry.flip_y) {
						add_unique_variant(transform_variant(src, [](int dx, int dy) { return std::pair(-dx, -dy); }));
					}
				}
			}

			for (const auto& v : cur.variants) {
				max_rx = std::max({max_rx, std::abs(static_cast<int>(v.min_dx)), std::abs(static_cast<int>(v.max_dx))});
				max_ry = std::max({max_ry, std::abs(static_cast<int>(v.min_dy)), std::abs(static_cast<int>(v.max_dy))});
			}

			rm.rules.push_back(cur);
		}

		runtime_materials[m.id] = rm;
	}

	Grid::set_max_rule_radius(max_rx, max_ry);

	if (Vulkan::is_available()) {
		Vulkan::update_rules();
		if (Grid::get_processing_mode() == ProcessingMode::GPU) {
			Grid::refresh_gpu_display();
		}
	}
}

const MaterialDefinition& MaterialManager::get_material(uint8_t id) {
	uint8_t idx = material_by_id[id];
	if (idx < materials.size() && materials[idx].id == id) {
		return materials[idx];
	}
	return default_empty;
}

const RuntimeMaterial& MaterialManager::get_runtime_material(uint8_t id) { return runtime_materials[id]; }

uint8_t MaterialManager::get_material_count() { return static_cast<uint8_t>(materials.size()); }

std::vector<MaterialDefinition>& MaterialManager::get_materials() { return materials; }