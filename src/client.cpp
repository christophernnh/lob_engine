#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "Protocol.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

// Global state
int sock_fd = -1;
int logged_in_user_id = -1;
enum class AppState { LOGIN, TRADING };
AppState current_state = AppState::LOGIN;

// --- Networking Helpers ---

bool connect_to_engine() {
    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/tmp/engine.sock", sizeof(addr.sun_path) - 1);

    if (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        return false;
    }
    return true;
}

bool attempt_login(const std::string& user, const std::string& pass) {
    // 1. Pack Header + Request
    MsgHeader header = { MsgType::LOGIN_REQ };
    LoginRequest req;
    strncpy(req.username, user.c_str(), 15);
    strncpy(req.password, pass.c_str(), 15);

    // 2. Send to Server
    send(sock_fd, &header, sizeof(header), 0);
    send(sock_fd, &req, sizeof(req), 0);

    // 3. Wait for Response
    LoginResponse res;
    if (recv(sock_fd, &res, sizeof(res), 0) > 0) {
        if (res.success) {
            logged_in_user_id = res.userId;
            return true;
        }
    }
    return false;
}

void send_order(double price, int qty, char side) {
    MsgHeader header = { MsgType::ORDER_NEW };
    OrderMsg msg;
    std::string clId = "TUI-" + std::to_string(rand() % 1000);
    strncpy(msg.clOrdId, clId.c_str(), 15);
    msg.price = price;
    msg.qty = qty;
    msg.side = side;

    send(sock_fd, &header, sizeof(header), 0);
    send(sock_fd, &msg, sizeof(msg), 0);
}

// --- TUI Components ---

Component CreateLoginScreen(std::function<void()> on_success) {
    static std::string username = "";
    static std::string password = "";
    static std::string status_msg = "Please log in";

    auto input_user = Input(&username, "Username");
    auto input_pass = Input(&password, "Password");
    
    // Mask password
    InputOption pass_opt;
    pass_opt.password = true;
    auto input_pass_masked = Input(&password, "Password", pass_opt);

    auto btn = Button("LOGIN", [&] {
        if (attempt_login(username, password)) {
            on_success();
        } else {
            status_msg = "Login Failed!";
        }
    });

    return Renderer(Container::Vertical({input_user, input_pass_masked, btn}), [&, input_user, input_pass_masked, btn] {
        return vbox({
            text("ENGINE LOGIN") | bold | center,
            separator(),
            hbox(text(" User: "), input_user->Render()),
            hbox(text(" Pass: "), input_pass_masked->Render()),
            separator(),
            center(btn->Render()),
            center(text(status_msg) | color(Color::Red))
        }) | border | size(WIDTH, LESS_THAN, 40) | center;
    });
}

Component CreateTradingScreen() {
    static std::string price_str = "100.0";
    static std::string qty_str = "10";
    
    auto input_price = Input(&price_str, "Price");
    auto input_qty = Input(&qty_str, "Quantity");

    auto btn_buy = Button("BUY (BID)", [&] {
        send_order(std::stod(price_str), std::stoi(qty_str), 'B');
    });

    auto btn_sell = Button("SELL (ASK)", [&] {
        send_order(std::stod(price_str), std::stoi(qty_str), 'S');
    });

    return Renderer(Container::Vertical({input_price, input_qty, btn_buy, btn_sell}), [&, input_price, input_qty, btn_buy, btn_sell] {
        return vbox({
            text("TRADING DASHBOARD - User ID: " + std::to_string(logged_in_user_id)) | bold | center,
            separator(),
            hbox(text(" Price: "), input_price->Render()),
            hbox(text(" Qty:   "), input_qty->Render()),
            separator(),
            hbox(btn_buy->Render() | flex, btn_sell->Render() | flex),
        }) | border | size(WIDTH, LESS_THAN, 60) | center;
    });
}

int main() {
    if (!connect_to_engine()) {
        std::cerr << "Error: Could not connect to /tmp/engine.sock. Is the server running?" << std::endl;
        return 1;
    }

    auto screen = ScreenInteractive::TerminalOutput();
    
    // State-based component switching
    Component login_view = CreateLoginScreen([&] { current_state = AppState::TRADING; });
    Component trading_view = CreateTradingScreen();

    auto main_container = Container::Tab({login_view, trading_view}, (int*)&current_state);

    screen.Loop(main_container);

    return 0;
}