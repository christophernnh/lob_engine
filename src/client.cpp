#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <vector>
#include "Protocol.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/table.hpp>

using namespace ftxui;

// --- Global State ---
int sock_fd = -1;
int logged_in_user_id = -1;
int selected_tab = 0; // Use a dedicated int for the Tab component

std::string global_login_msg = "Please log in";
bool login_in_progress = false;

std::mutex snapshot_mutex;
MarketDataSnapshot current_snapshot;

// --- Networking Helpers ---

bool connect_to_engine()
{
    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/tmp/engine.sock", sizeof(addr.sun_path) - 1);

    return (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) != -1);
}

void socket_listener(ScreenInteractive *screen)
{
    while (true)
    {
        MsgHeader header;
        // 1. Read header (MSG_WAITALL ensures we get exactly 4/8 bytes)
        if (recv(sock_fd, &header, sizeof(header), MSG_WAITALL) <= 0)
            break;

        if (header.type == MsgType::LOGIN_RES)
        {
            LoginResponse res;
            // Note: If your LoginResponse struct DOES NOT contain the header,
            // read the rest of it here.
            if (recv(sock_fd, &res, sizeof(res), MSG_WAITALL) > 0)
            {
                if (res.success)
                {
                    logged_in_user_id = res.userId;
                    global_login_msg = "Success!";
                    selected_tab = 1; // Flip to TRADING screen
                }
                else
                {
                    global_login_msg = "Login Failed: Invalid Credentials";
                    login_in_progress = false;
                }
                screen->PostEvent(Event::Custom);
            }
        }
        else if (header.type == MsgType::MARKET_DATA)
        {
            MarketDataSnapshot snap;
            // Since we already read the header, we read the REST of the snapshot struct
            // This assumes your struct starts with the header.
            char *ptr = (char *)&snap + sizeof(MsgHeader);
            int remaining = sizeof(MarketDataSnapshot) - sizeof(MsgHeader);

            if (recv(sock_fd, ptr, remaining, MSG_WAITALL) == remaining)
            {
                std::lock_guard<std::mutex> lock(snapshot_mutex);
                current_snapshot = snap;
                current_snapshot.header = header; // Restore the header we read earlier
                screen->PostEvent(Event::Custom);
            }
        }
    }
}

void attempt_login(const std::string &user, const std::string &pass)
{
    login_in_progress = true;
    global_login_msg = "Authenticating...";

    MsgHeader header = {MsgType::LOGIN_REQ};
    LoginRequest req;
    strncpy(req.username, user.c_str(), 15);
    strncpy(req.password, pass.c_str(), 15);

    send(sock_fd, &header, sizeof(header), 0);
    send(sock_fd, &req, sizeof(req), 0);
}

void send_order(double price, int qty, char side)
{
    MsgHeader header = {MsgType::ORDER_NEW};
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

Component CreateLoginScreen()
{
    static std::string username = "";
    static std::string password = "";

    auto input_user = Input(&username, "Username");
    InputOption pass_opt;
    pass_opt.password = true;
    auto input_pass = Input(&password, "Password", pass_opt);

    auto btn = Button("LOGIN", [&]
                      {
        if (!login_in_progress) attempt_login(username, password); }, ButtonOption::Ascii());

    return Renderer(Container::Vertical({input_user, input_pass, btn}), [&, input_user, input_pass, btn]
                    { return vbox({text("EXCHANGE LOGIN") | bold | center,
                                   separator(),
                                   hbox(text(" User: "), input_user->Render()) | border,
                                   hbox(text(" Pass: "), input_pass->Render()) | border,
                                   separator(),
                                   center(btn->Render()),
                                   center(text(global_login_msg) | color(login_in_progress ? Color::Yellow : Color::Red))}) |
                             border | size(WIDTH, LESS_THAN, 50) | center; });
}

Element RenderOrderBook(const MarketDataSnapshot &snapshot)
{
    std::vector<std::vector<Element>> data;
    data.push_back({text("Qty") | bold, text("Bid") | color(Color::Green), text(" "), text("Ask") | color(Color::Red), text("Qty") | bold});

    for (int i = 0; i < 10; ++i)
    {
        bool has_bid = i < snapshot.bidLevels;
        bool has_ask = i < snapshot.askLevels;
        data.push_back({text(has_bid ? std::to_string(snapshot.bids[i].volume) : ""),
                        text(has_bid ? std::to_string(snapshot.bids[i].price) : "") | color(Color::Green),
                        text("|") | dim,
                        text(has_ask ? std::to_string(snapshot.asks[i].price) : "") | color(Color::Red),
                        text(has_ask ? std::to_string(snapshot.asks[i].volume) : "")});
    }

    auto table = Table(data);
    table.SelectAll().SeparatorVertical();
    table.SelectRow(0).SeparatorHorizontal();
    table.SelectAll().Border(LIGHT);
    return table.Render();
}

Component CreateTradingScreen()
{
    static std::string price_str = "100.0";
    static std::string qty_str = "10";

    auto input_price = Input(&price_str, "Price");
    auto input_qty = Input(&qty_str, "Qty");

    auto btn_buy = Button("BUY", []
                          { 
        try {
            send_order(std::stod(price_str), std::stoi(qty_str), 'B'); 
        } catch (...) {} }, ButtonOption::Ascii());

    auto btn_sell = Button("SELL", []
                           { 
        try {
            send_order(std::stod(price_str), std::stoi(qty_str), 'S'); 
        } catch (...) {} }, ButtonOption::Ascii());

    auto container = Container::Vertical({
        input_price,
        input_qty,
        btn_buy,
        btn_sell,
    });

    return Renderer(container, [input_price, input_qty, btn_buy, btn_sell]
                    {
        auto book_panel = vbox({
            text(" MARKET DEPTH ") | bold | center,
            RenderOrderBook(current_snapshot)
        }) | flex;

        auto order_panel = vbox({
            text(" ORDER ENTRY ") | bold | center,
            separator(),
            hbox(text(" Price: "), input_price->Render()) | border,
            hbox(text(" Qty:   "), input_qty->Render()) | border,
            hbox(btn_buy->Render() | flex | color(Color::Green), 
                 btn_sell->Render() | flex | color(Color::Red))
        }) | size(WIDTH, GREATER_THAN, 35);

        return hbox({ book_panel, separator(), order_panel }) | borderDouble; });
}

int main()
{
    if (!connect_to_engine())
        return 1;

    auto screen = ScreenInteractive::TerminalOutput();

    // Start listener thread immediately
    std::thread([&]
                { socket_listener(&screen); })
        .detach();

    auto login_view = CreateLoginScreen();
    auto trading_view = CreateTradingScreen();

    // Use the dedicated 'selected_tab' int
    auto main_tabs = Container::Tab({login_view, trading_view}, &selected_tab);

    // Main Renderer that forces a refresh of the Tab container
    auto main_renderer = Renderer(main_tabs, [&]
                                  { return main_tabs->Render(); });

    screen.Loop(main_renderer);
    return 0;
}