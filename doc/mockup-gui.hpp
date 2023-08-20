



void NewInstance(TGUI& gui)
{
	gui.Design(TPath(L"gui/design/test-screen")); // the renderer will append a subfolder with the name of the renderer (e.g.: www)

	auto btn_ok = gui.CreateButton("ok-button", [](TButton& btn){
		// on click
	}));

	btn_ok.Enabled(false);

	gui.CreateButton("cancel-button", [](TButton& btn){
		// on click
	});

	gui.AddTextbox("first-name", [](TTextbox& txt){
		btn_ok.Enabled(txt.Text().Length() > 0);
	});
}
