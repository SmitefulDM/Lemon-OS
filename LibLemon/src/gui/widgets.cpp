#include <gui/widgets.h>

#include <core/keyboard.h>
#include <ctype.h>
#include <string>
#include <math.h>
#include <gui/colours.h>
#include <gui/window.h>
#include <assert.h>
#include <algorithm>

namespace Lemon::GUI {
    Widget::Widget() {}

    Widget::Widget(rect_t bounds, LayoutSize newSizeX, LayoutSize newSizeY){
        this->bounds = bounds;

        sizeX = newSizeX;
        sizeY = newSizeY;

        UpdateFixedBounds();
    }

    Widget::~Widget(){}

    void Widget::Paint(__attribute__((unused)) surface_t* surface){}

    void Widget::OnMouseDown(__attribute__((unused)) vector2i_t mousePos){}

    void Widget::OnMouseUp(__attribute__((unused)) vector2i_t mousePos){}

    void Widget::OnRightMouseDown(__attribute__((unused)) vector2i_t mousePos){}

    void Widget::OnRightMouseUp(__attribute__((unused)) vector2i_t mousePos){}

    void Widget::OnHover(__attribute__((unused)) vector2i_t mousePos){}

    void Widget::OnMouseMove(__attribute__((unused)) vector2i_t mousePos) {}
    
    void Widget::OnDoubleClick(__attribute__((unused)) vector2i_t mousePos) {}

    void Widget::OnKeyPress(__attribute__((unused)) int key) {}

    void Widget::OnCommand(__attribute__((unused)) unsigned short key) {}

    void Widget::UpdateFixedBounds(){
        fixedBounds.pos = bounds.pos;

        if(parent){
            fixedBounds.pos += parent->GetFixedBounds().pos;

            if(sizeX == LayoutSize::Stretch){
                if(align == WidgetAlignment::WAlignRight)
                    fixedBounds.width = (parent->GetFixedBounds().size.x - bounds.pos.x) - parent->GetFixedBounds().pos.x;
                else
                    fixedBounds.width = parent->GetFixedBounds().width - bounds.width - bounds.x;
            } else {
                fixedBounds.width = bounds.width;
            }
            
            if(sizeY == LayoutSize::Stretch){
                if(verticalAlign == WidgetAlignment::WAlignBottom)
                    fixedBounds.height = (parent->GetFixedBounds().height - bounds.y) - parent->GetFixedBounds().y;
                else
                    fixedBounds.height = parent->GetFixedBounds().height - bounds.height - bounds.y;
            } else {
                fixedBounds.height = bounds.height;
            }

            if(align == WidgetAlignment::WAlignRight){
                fixedBounds.pos.x = (parent->GetFixedBounds().pos.x + parent->GetFixedBounds().size.x) - bounds.pos.x - fixedBounds.width;
            } else if(align == WAlignCentre){
                fixedBounds.pos.x = parent->GetFixedBounds().width / 2 - fixedBounds.width / 2 + bounds.x;
            }

            if(verticalAlign == WidgetAlignment::WAlignBottom){
                fixedBounds.pos.y = (parent->GetFixedBounds().y + parent->GetFixedBounds().height) - bounds.y - fixedBounds.height;
            } else if(verticalAlign == WAlignCentre){
                fixedBounds.pos.y = parent->GetFixedBounds().height / 2 - fixedBounds.height / 2 + bounds.y;
            }
        } else {
            fixedBounds.size = bounds.size;
        }
    }

    //////////////////////////
    // Contianer
    //////////////////////////
    Container::Container(rect_t bounds) : Widget(bounds) {

    }

    void Container::AddWidget(Widget* w){
        children.push_back(w);

        w->SetParent(this);
        w->window = window;
        
        UpdateFixedBounds();
    }

    void Container::RemoveWidget(Widget* w){
        w->SetParent(nullptr);
        w->window = nullptr;

        children.erase(std::remove(children.begin(), children.end(), w), children.end());
    }

    void Container::Paint(surface_t* surface){
        if(background.a == 255) Graphics::DrawRect(fixedBounds, background, surface);

        for(Widget* w : children){
            w->Paint(surface);
        }
    }

    void Container::OnMouseDown(vector2i_t mousePos){
        for(Widget* w : children){
            if(Graphics::PointInRect(w->GetFixedBounds(), mousePos)){
                active = w;
                w->OnMouseDown(mousePos);
                break;
            }
        }
    }

    void Container::OnMouseUp(vector2i_t mousePos){
        if(active){
            active->OnMouseUp(mousePos);
        }
    }

    void Container::OnRightMouseDown(vector2i_t mousePos){
        for(Widget* w : children){
            if(Graphics::PointInRect(w->GetFixedBounds(), mousePos)){
                active = w;
                w->OnRightMouseDown(mousePos);
                break;
            }
        }
    }

    void Container::OnRightMouseUp(vector2i_t mousePos){
        if(active){
            active->OnRightMouseUp(mousePos);
        }
    }

    void Container::OnMouseMove(vector2i_t mousePos){
        if(active){
            active->OnMouseMove(mousePos);
        }
    }

    void Container::OnDoubleClick(vector2i_t mousePos){
        if(active && Graphics::PointInRect(active->GetFixedBounds(), mousePos)){ // If user hasnt clicked on same widget then this aint a double click
            active->OnDoubleClick(mousePos);
        } else {
            OnMouseDown(mousePos);
        }
    }

    void Container::OnKeyPress(int key){
        if(active) {
            active->OnKeyPress(key);
        }
    }

    void Container::UpdateFixedBounds(){
        Widget::UpdateFixedBounds();

        for(Widget* w : children){
            w->UpdateFixedBounds();
        }
    }
    
    //////////////////////////
    // LayoutContainer
    //////////////////////////
    LayoutContainer::LayoutContainer(rect_t bounds, vector2i_t itemSize) : Container(bounds){
        this->itemSize = itemSize;
    }

    void LayoutContainer::AddWidget(Widget* w){
        Container::AddWidget(w);

        UpdateFixedBounds();
    }

    void LayoutContainer::RemoveWidget(Widget* w){
        Container::RemoveWidget(w);
        
        UpdateFixedBounds();
    }
    
    void LayoutContainer::UpdateFixedBounds(){
        Widget::UpdateFixedBounds();

        vector2i_t pos = {2, 2};
        isOverflowing = false;

        for(Widget* w : children){
            w->SetBounds({pos, itemSize});
            w->SetLayout(LayoutSize::Fixed, LayoutSize::Fixed, WidgetAlignment::WAlignLeft);

            w->UpdateFixedBounds();

            pos.x += itemSize.x;

            if(pos.x + itemSize.x > fixedBounds.width){
                pos.x = 2;
                pos.y += itemSize.y + 2;
            }
        }

        if(pos.y + itemSize.y > fixedBounds.height) isOverflowing = true;
    }

    //////////////////////////
    // Button
    //////////////////////////
    Button::Button(const char* _label, rect_t _bounds) : Widget(_bounds) {
        label = _label;
        labelLength = Graphics::GetTextLength(label.c_str());
    }

    void Button::DrawButtonBorders(surface_t* surface, bool white){
        vector2i_t btnPos = fixedBounds.pos;
        rect_t bounds = fixedBounds;

        if(white){
            Graphics::DrawRect(bounds.pos.x+1, btnPos.y, bounds.size.x - 2, 1, 250, 250, 250, surface);
            Graphics::DrawRect(btnPos.x+1, btnPos.y + bounds.size.y - 1, bounds.size.x - 2, 1, 250, 250, 250, surface);
            Graphics::DrawRect(btnPos.x, btnPos.y + 1, 1, bounds.size.y - 2, 250, 250, 250, surface);
            Graphics::DrawRect(btnPos.x + bounds.size.x - 1, btnPos.y + 1, 1, bounds.size.y-2, 250, 250, 250, surface);
        } else {
            Graphics::DrawRect(btnPos.x+1, btnPos.y, bounds.size.x - 2, 1, 96, 96, 96, surface);
            Graphics::DrawRect(btnPos.x+1, btnPos.y + bounds.size.y - 1, bounds.size.x - 2, 1, 96, 96, 96, surface);
            Graphics::DrawRect(btnPos.x, btnPos.y + 1, 1, bounds.size.y - 2, 96, 96, 96, surface);
            Graphics::DrawRect(btnPos.x + bounds.size.x - 1, btnPos.y + 1, 1, bounds.size.y-2, 96, 96, 96, surface);
        }
    }

    void Button::DrawButtonLabel(surface_t* surface, bool white){
        rgba_colour_t colour;
        vector2i_t btnPos = fixedBounds.pos;

        if(white){
            colour = colours[Colour::TextAlternate];
        } else {
            colour = colours[Colour::Text];
        }

        if(labelAlignment == TextAlignment::Centre){
            Graphics::DrawString(label.c_str(), btnPos.x + (fixedBounds.size.x / 2) - (labelLength / 2), btnPos.y + (bounds.size.y / 2 - 8), colour.r, colour.g, colour.b, surface);
        } else {
            Graphics::DrawString(label.c_str(), btnPos.x + 2, btnPos.y + (fixedBounds.size.y / 2 - 6), colour.r, colour.g, colour.b, surface);
        }
    }

    void Button::Paint(surface_t* surface){
        vector2i_t btnPos = fixedBounds.pos;
        rect_t bounds = fixedBounds;

        if(pressed){
            Graphics::DrawRect(btnPos.x+1, btnPos.y+1, bounds.size.x-2, (bounds.size.y)/2 - 1, 224/1.1, 224/1.1, 219/1.1, surface);
            Graphics::DrawRect(btnPos.x+1, btnPos.y + bounds.size.y / 2, bounds.size.x-2, bounds.size.y/2-1, 224/1.1, 224/1.1, 219/1.1, surface);

            DrawButtonBorders(surface, false);
        } else {
            switch(style){
                case 1: // Blue
                    Graphics::DrawGradientVertical(btnPos.x + 1, btnPos.y + 1, bounds.size.x - 2, bounds.size.y - 2,{50,50,150,255},{45,45,130,255},surface);
                    DrawButtonBorders(surface, true);
                    if(drawText) DrawButtonLabel(surface, true);
                    break;
                case 2: // Red
                    Graphics::DrawGradientVertical(btnPos.x + 1, btnPos.y + 1, bounds.size.x - 2, bounds.size.y - 2,{180,60,60,255},{160,55,55,255},surface);
                    DrawButtonBorders(surface, true);
                    if(drawText) DrawButtonLabel(surface, true);
                    break;
                case 3: // Yellow
                    Graphics::DrawGradientVertical(btnPos.x + 1, btnPos.y + 1, bounds.size.x - 2, bounds.size.y - 2,{200,160,50,255},{180,140,45,255},surface);
                    DrawButtonBorders(surface, true);
                    if(drawText) DrawButtonLabel(surface, true);
                    break;
                default:
                    Graphics::DrawRect(btnPos.x + 1, btnPos.y + 1, bounds.size.x - 2, bounds.size.y - 4, colours[Colour::ContentBackground],surface);
                    Graphics::DrawGradientVertical(btnPos.x + 1, btnPos.y + bounds.size.y - 5, bounds.size.x - 2, 4,colours[Colour::ContentBackground],colours[Colour::Background],surface);
                    //Graphics::DrawRect(btnPos.x + 1, btnPos.y + bounds.size.y - 3, bounds.size.x - 2, 2, colours[Colour::ContentShadow],surface);
                    DrawButtonBorders(surface, false);
                    if(drawText) DrawButtonLabel(surface, false);
                    break;
            }

            if(Graphics::PointInRect(fixedBounds, window->lastMousePos)){
                Graphics::DrawRectOutline(fixedBounds.x + 1, fixedBounds.y + 1, fixedBounds.width - 2, fixedBounds.height - 2, colours[Colour::Foreground], surface);
                Graphics::DrawRectOutline(fixedBounds.x + 2, fixedBounds.y + 2, fixedBounds.width - 4, fixedBounds.height - 4, colours[Colour::Foreground], surface);
            }
        }
    }

    void Button::OnMouseDown(__attribute__((unused)) vector2i_t mousePos){
        pressed = true;
    }

    void Button::OnMouseUp(vector2i_t mousePos){
        if(Graphics::PointInRect(fixedBounds, mousePos) && OnPress) OnPress(this);

        pressed = false;
    }

    //////////////////////////
    // Bitmap
    //////////////////////////

    Bitmap::Bitmap(rect_t _bounds) : Widget(_bounds) {
        surface.buffer = (uint8_t*)malloc(fixedBounds.size.x*fixedBounds.size.y*4);
        surface.width = fixedBounds.size.x;
        surface.height = fixedBounds.size.y;
    }
    
    Bitmap::Bitmap(rect_t _bounds, surface_t* surf) : Widget({_bounds.pos, (vector2i_t){surf->width, surf->height}}){
        surface = *surf;
    }

    void Bitmap::Paint(surface_t* surface){
        Graphics::surfacecpy(surface, &this->surface, fixedBounds.pos);
    }

    //////////////////////////
    // Label
    //////////////////////////
    Label::Label(const char* _label, rect_t _bounds) : Widget(_bounds) {
        label = _label;
    }

    void Label::Paint(surface_t* surface){
        Graphics::DrawString(label.c_str(), fixedBounds.pos.x, fixedBounds.pos.y, textColour.r, textColour.g, textColour.b, surface);
    }

    //////////////////////////
    // Scroll Bar
    //////////////////////////
    void ScrollBar::ResetScrollBar(int displayHeight, int areaHeight){
        double scrollDisplayRange = ((double)areaHeight) / displayHeight; // Essentially how many 'displayHeight's in areaHeight
        scrollIncrement = ceil(scrollDisplayRange);
        scrollBar.size.y = displayHeight / scrollDisplayRange;
        scrollBar.pos.y = 0;
        scrollPos = 0;
        height = displayHeight;
    }

    void ScrollBar::Paint(surface_t* surface, vector2i_t offset, int width){
        Graphics::DrawRect(offset.x, offset.y, width, height, 128, 128, 128, surface);
        if(pressed) 
            Graphics::DrawRect(offset.x, offset.y + scrollBar.pos.y, width, scrollBar.size.y, 224/1.1, 224/1.1, 219/1.1, surface);
        else 
            Graphics::DrawGradientVertical(offset.x, offset.y + scrollBar.pos.y, width, scrollBar.size.y,{250,250,250,255},{235,235,230,255},surface);
    }

    void ScrollBar::OnMouseDownRelative(vector2i_t mousePos){
        if(mousePos.y > scrollBar.pos.y && mousePos.y < scrollBar.pos.y + scrollBar.size.y){
            pressed = true;
            pressOffset = mousePos.y - scrollBar.pos.y;
        }
    }

    void ScrollBar::OnMouseMoveRelative(vector2i_t relativePosition){
        if(pressed){
            scrollBar.pos.y = relativePosition.y - pressOffset;
            if(scrollBar.pos.y + scrollBar.size.y > height) scrollBar.pos.y = height - scrollBar.size.y;
            if(scrollBar.pos.y < 0) scrollBar.pos.y = 0;
            scrollPos = scrollBar.pos.y * scrollIncrement;
        }
    }

    void ScrollBarHorizontal::ResetScrollBar(int displayWidth, int areaWidth){
        double scrollDisplayRange = ((double)areaWidth) / displayWidth; // Essentially how many 'displayHeight's in areaHeight
        scrollIncrement = ceil(scrollDisplayRange);
        scrollBar.size.x = displayWidth / scrollDisplayRange;
        scrollBar.pos.x = 0;
        scrollPos = 0;
        width = displayWidth;
    }

    void ScrollBarHorizontal::Paint(surface_t* surface, vector2i_t offset, int height){
        Graphics::DrawRect(offset.x, offset.y, width, height, 128, 128, 128, surface);
        
        if(pressed) 
            Graphics::DrawRect(offset.x + scrollBar.pos.x, offset.y, scrollBar.size.x, height, 224/1.1, 224/1.1, 219/1.1, surface);
        else 
            Graphics::DrawGradient(offset.x + scrollBar.pos.x, offset.y, scrollBar.size.x, height,{250,250,250,255},{235,235,230,255},surface);
    }

    void ScrollBarHorizontal::OnMouseDownRelative(vector2i_t mousePos){
        if(mousePos.x > scrollBar.pos.x && mousePos.x < scrollBar.pos.x + scrollBar.size.x){
            pressed = true;
            pressOffset = mousePos.x - scrollBar.pos.x;
        }
    }

    void ScrollBarHorizontal::OnMouseMoveRelative(vector2i_t relativePosition){
        if(pressed){
            scrollBar.pos.x = relativePosition.x - pressOffset;
            if(scrollBar.pos.x + scrollBar.size.x > width) scrollBar.pos.x = width - scrollBar.size.x;
            if(scrollBar.pos.x < 0) scrollBar.pos.x = 0;
            scrollPos = scrollBar.pos.x * scrollIncrement;
        }
    }

    //////////////////////////
    // Text Box
    //////////////////////////
    TextBox::TextBox(rect_t bounds, bool multiline) : Widget(bounds) {
        this->multiline = multiline;
        font = Graphics::GetFont("default");
        contents.push_back(std::string());

        {
            ContextMenuEntry ctx;
            ctx.id = TextboxCommandCut;
            ctx.name = "Cut";

            ctxEntries.push_back(ctx);
        }

        {
            ContextMenuEntry ctx;
            ctx.id = TextboxCommandCopy;
            ctx.name = "Copy";

            ctxEntries.push_back(ctx);
        }

        {
            ContextMenuEntry ctx;
            ctx.id = TextboxCommandPaste;
            ctx.name = "Paste";

            ctxEntries.push_back(ctx);
        }

        {
            ContextMenuEntry ctx;
            ctx.id = TextboxCommandDelete;
            ctx.name = "Delete";

            ctxEntries.push_back(ctx);
        }
    }

    void TextBox::Paint(surface_t* surface){
        Graphics::DrawRect(fixedBounds.pos.x + 1, fixedBounds.pos.y + 1, fixedBounds.size.x - 2, fixedBounds.size.y - 2, colours[Colour::ContentBackground], surface);
        Graphics::DrawRectOutline(fixedBounds, colours[Colour::ContentShadow], surface);
        int xpos = 2;
        int ypos = 2;
        int curYOffset = 0;

        if(multiline){
            curYOffset = cursorPos.y * (font->height + lineSpacing) - 1 - sBar.scrollPos + 2;

            for(size_t i = 0; i < contents.size(); i++){
                for(size_t j = 0; j < contents[i].length(); j++){
                    if(contents[i][j] == '\t'){
                        xpos += font->tabWidth * font->width;
                        continue;
                    } else if (isspace(contents[i][j])) {
                        xpos += font->width;
                        continue;
                    }
                    else if (!isgraph(contents[i][j])) continue;

                    xpos += Graphics::DrawChar(contents[i][j], fixedBounds.pos.x + xpos, fixedBounds.pos.y + ypos - sBar.scrollPos, textColour.r, textColour.g, textColour.b, surface, font);

                    if((xpos > (fixedBounds.size.x - 8 - 16))){
                        //xpos = 0;
                        //ypos += font->height + lineSpacing;
                        break;
                    }
                }
                ypos += font->height + lineSpacing;
                xpos = 0;
                if(ypos - sBar.scrollPos + font->height + lineSpacing >= fixedBounds.size.y) break;
            }

            sBar.Paint(surface, {fixedBounds.pos.x + fixedBounds.size.x - 16, fixedBounds.pos.y});
        } else {
            ypos = fixedBounds.height / 2 - font->height / 2;
            curYOffset = ypos;

            std::string& line = contents.front();
            for(size_t j = 0; j < contents[0].length(); j++){
                char ch;
                
                if(masked){
                    ch = '*';
                } else {
                    ch = line[j];

                    if(ch == '\t'){
                        xpos += font->tabWidth * font->width;
                        continue;
                    } else if (isspace(ch)) {
                        xpos += font->width;
                        continue;
                    }
                    else if (!isgraph(ch)) continue;
                }

                xpos += Graphics::DrawChar(ch, fixedBounds.pos.x + xpos, fixedBounds.pos.y + ypos, textColour.r, textColour.g, textColour.b, surface, font);
            }
        }

        if(parent->active == this){ // Only draw cursor if active
            timespec t;
            clock_gettime(CLOCK_BOOTTIME, &t);

            long msec = (t.tv_nsec / 1000000.0);
            if(msec < 250 || (msec > 500 && msec < 750)) // Only draw the cursor for a quarter of a second so it blinks
                Graphics::DrawRect(fixedBounds.pos.x + Graphics::GetTextLength(contents[cursorPos.y].c_str(), cursorPos.x, font) + 2, fixedBounds.pos.y + curYOffset, 2, font->height + 2, textColour.r, textColour.g, textColour.b, surface);
        }
    }

    void TextBox::LoadText(const char* text){
        const char* text2 = text;
        int lineCount = 1;

        contents.clear();

        if(multiline){
            while(*text2){
                if(*text2 == '\n') lineCount++;
                text2++;
            }

            text2 = text;
            for(int i = 0; i < lineCount; i++){
                char* end = strchr(text2, '\n');
                if(end){
                    size_t len = (uintptr_t)(end - text2);
                    if(len > strlen(text2)) break;

                    contents.push_back(std::string(text2, len));

                    text2 = end + 1;
                } else {
                    contents.push_back(std::string(text2));
                }
            }
            
            this->lineCount = contents.size();
            ResetScrollBar();
        } else {
            contents.push_back(std::string(text2));
        }
    }

    void TextBox::OnMouseDown(vector2i_t mousePos){
        assert(contents.size() <= INT_MAX);

        mousePos.x -= fixedBounds.pos.x;
        mousePos.y -= fixedBounds.pos.y;

        if(multiline && mousePos.x > fixedBounds.size.x - 16){
            sBar.OnMouseDownRelative({mousePos.x + fixedBounds.size.x - 16, mousePos.y});
            return;
        }

        if(multiline){
            cursorPos.y = (sBar.scrollPos + mousePos.y - 2 + lineSpacing / 2) / (font->height + lineSpacing);
            if(cursorPos.y >= static_cast<int>(contents.size())) cursorPos.y = contents.size() - 1;
        }

        int dist = 0;
        for(cursorPos.x = 0; cursorPos.x < static_cast<int>(contents[cursorPos.y].length()); cursorPos.x++){
            dist += Graphics::GetCharWidth(contents[cursorPos.y][cursorPos.x], font);
            if(dist >= mousePos.x){
                break;
            }
        }
    }

    void TextBox::OnRightMouseDown(__attribute__((unused)) vector2i_t mousePos){
        if(window){
            window->DisplayContextMenu(ctxEntries);
        }
    }

    void TextBox::OnCommand(unsigned short cmd){
        (void)cmd;
    }

    void TextBox::OnMouseMove(__attribute__((unused)) vector2i_t mousePos){
        if(multiline && sBar.pressed){
            sBar.OnMouseMoveRelative({0, mousePos.y - fixedBounds.pos.y});
        }
    }

    void TextBox::OnMouseUp(__attribute__((unused)) vector2i_t mousePos){
        sBar.pressed = false;
    }


    void TextBox::ResetScrollBar(){
        sBar.ResetScrollBar(fixedBounds.size.y, contents.size() * (font->height + lineSpacing));
    }

    void TextBox::OnKeyPress(int key){
        if(!editable) return;

        assert(contents.size() < INT_MAX);

        if(isprint(key)){
            contents[cursorPos.y].insert(cursorPos.x++, 1, key);
        } else if(key == '\b' || key == KEY_DELETE){
            if(key == KEY_DELETE){ // Delete is essentially backspace but on the character in front so just increment the cursor pos.
                cursorPos.x++;
                if(cursorPos.x > static_cast<int>(contents[cursorPos.y].length())){
                    if(cursorPos.y < static_cast<int>(contents.size())){
                        cursorPos.x = static_cast<int>(contents[++cursorPos.y].length());
                    } else cursorPos.x = static_cast<int>(contents[cursorPos.y].length());
                }
            }

            if(cursorPos.x) {
                contents[cursorPos.y].erase(--cursorPos.x, 1);
            } else if(cursorPos.y) { // Delete line if not at start of file
                cursorPos.x = static_cast<int>(contents[cursorPos.y - 1].length()); // Move cursor horizontally to end of previous line
                contents[cursorPos.y - 1] += contents[cursorPos.y]; // Append contents of current line to previous
                contents.erase(contents.begin() + cursorPos.y--); // Remove line and move to previous line

                ResetScrollBar();
            }
        } else if(key == '\n'){
            if(multiline){
                std::string s = contents[cursorPos.y].substr(cursorPos.x); // Grab the contents of the line after the cursor
                contents[cursorPos.y].erase(cursorPos.x); // Erase all that was after the cursor
                contents.insert(contents.begin() + (++cursorPos.y), s); // Insert new line after cursor and move to that line
                cursorPos.x = 0;
                ResetScrollBar();
            } else if (OnSubmit){
                OnSubmit(this);
            }
        } else if (key == KEY_ARROW_LEFT){ // Move cursor left
            cursorPos.x--;
            if(cursorPos.x < 0){
                if(cursorPos.y){
                    cursorPos.x = static_cast<int>(contents[--cursorPos.y].length());
                } else cursorPos.x = 0;
            }
        } else if (key == KEY_ARROW_RIGHT){ // Move cursor right
            cursorPos.x++;
            if(cursorPos.x > static_cast<int>(contents[cursorPos.y].length())){
                if(cursorPos.y < static_cast<int>(contents.size())){
                    cursorPos.x = contents[++cursorPos.y].length();
                } else cursorPos.x = contents[cursorPos.y].length();
            }
        } else if (key == KEY_ARROW_UP){ // Move cursor up
            if(cursorPos.y){
                cursorPos.y--;
                if(cursorPos.x > static_cast<int>(contents[cursorPos.y].length())){
                    cursorPos.x = static_cast<int>(contents[cursorPos.y].length());
                }
            } else cursorPos.x = 0;
        } else if (key == KEY_ARROW_DOWN){ // Move cursor down
            if(cursorPos.y < static_cast<int>(contents.size())){
                cursorPos.y++;
                if(cursorPos.x > static_cast<int>(contents[cursorPos.y].length())){
                    cursorPos.x = static_cast<int>(contents[cursorPos.y].length());
                }
            } else cursorPos.x = contents[cursorPos.y].length();
        }
    }

    void TextBox::MaskText(bool state){
        if(!multiline){
            masked = state;
        } else {
            masked = false;
        }
    }

    //////////////////////////
    // ListView
    //////////////////////////
    ListView::ListView(rect_t bounds) : Widget(bounds) {
        font = Graphics::GetFont("default");

        assert(font); // Then shit really went down
    }

    ListView::~ListView(){

    }

    void ListView::Paint(surface_t* surface){
        assert(items.size() < INT_MAX);

        Graphics::DrawRect(fixedBounds.x, fixedBounds.y, fixedBounds.width, columnDisplayHeight, colours[Colour::Background], surface);
        rgba_colour_t textColour = colours[Colour::Text];
        
        int totalColumnWidth;
        int xPos = fixedBounds.x;
        for(ListColumn col : columns){
            Graphics::DrawString(col.name.c_str(), xPos + 4, fixedBounds.y + 4, textColour.r, textColour.g, textColour.b, surface);

            xPos += col.displayWidth;

            Graphics::DrawRect(xPos, fixedBounds.y + 1, 1, columnDisplayHeight - 2, textColour, surface); // Divider
            xPos++;
            Graphics::DrawRect(xPos, fixedBounds.y + 1, 1, columnDisplayHeight - 2, colours[Colour::TextAlternate], surface); // Divider
            xPos++;
        }

        totalColumnWidth = xPos;

        int yPos = fixedBounds.y + columnDisplayHeight;

        Graphics::DrawRect(fixedBounds.x, fixedBounds.y + columnDisplayHeight, fixedBounds.width, fixedBounds.height - 20, colours[Colour::ContentBackground], surface);

        int index = 0;

        if(showScrollBar) index = sBar.scrollPos / itemHeight;
        int maxItem = index + fixedBounds.height / itemHeight;

        for(; index < static_cast<int>(items.size()) && index < maxItem; index++){
            ListItem item = items[index];

            xPos = fixedBounds.x;

            for(unsigned i = 0; i < item.details.size() && i < columns.size(); i++){

                std::string str = item.details[i];

                if(Graphics::GetTextLength(str.c_str()) > columns[i].displayWidth - 2) {
                    int l = str.length() - 1;
                    while(l){
                        str = str.substr(0, l);

                        if(Graphics::GetTextLength(str.c_str()) < columns[i].displayWidth + 2) {
                            if(l > 2){
                                str.erase(str.end() - 1); // Omit last character
                                str.append("..."); // We have a variable width font should we should only have to omit 1 character
                            }
                            break;
                        }
                        
                        l = str.length() - 1;
                    }
                }

                vector2i_t textPos = {xPos + 2, yPos + itemHeight / 2 - font->height / 2};

                if(index == selected){
                    Graphics::DrawRect(xPos + 1, yPos + 1, totalColumnWidth - 2, itemHeight - 2, colours[Colour::Foreground], surface);
                    Graphics::DrawString(str.c_str(), textPos.x, textPos.y, colours[Colour::TextAlternate], surface, fixedBounds);
                } else {
                    Graphics::DrawString(str.c_str(), textPos.x, textPos.y, textColour.r, textColour.g, textColour.b, surface, fixedBounds);
                }

                xPos += columns[i].displayWidth + 2;
            }

            yPos += itemHeight;
        }

        if(showScrollBar) sBar.Paint(surface, fixedBounds.pos + (vector2i_t){fixedBounds.size.x, 0} - (vector2i_t){16, -columnDisplayHeight});
    }

    void ListView::AddColumn(ListColumn& column){
        columns.push_back(ListColumn(column));
    }

    int ListView::AddItem(ListItem& item){
        int index = items.size();

        items.push_back(ListItem(item));

        ResetScrollBar();

        return index;
    }

    void ListView::ClearItems(){
        items.clear();

        ResetScrollBar();
    }
    
    void ListView::OnMouseDown(vector2i_t mousePos){
        assert(items.size() < INT32_MAX);

        if(showScrollBar && mousePos.x > fixedBounds.pos.x + fixedBounds.size.x - 16){
            sBar.OnMouseDownRelative({mousePos.x - fixedBounds.pos.x + fixedBounds.size.x - 16, mousePos.y - columnDisplayHeight - fixedBounds.pos.y});
            return;
        }

        selected = floor(((double)mousePos.y + sBar.scrollPos - fixedBounds.pos.y - columnDisplayHeight) / itemHeight);

        if(selected < 0) selected = 0;
        if(selected >= (int)items.size()) selected = (int)items.size() - 1;

        if(OnSelect) OnSelect(items[selected], this);
    }

    void ListView::OnDoubleClick(vector2i_t mousePos){
        assert(items.size() < INT32_MAX);

        if(!Graphics::PointInRect({fixedBounds.x, fixedBounds.y + columnDisplayHeight, fixedBounds.width - (showScrollBar ? 16 : 0), fixedBounds.height - columnDisplayHeight}, mousePos)){
            OnMouseDown(mousePos);
            return;
        } else {
            int clickedItem = floor(((double)mousePos.y + sBar.scrollPos - fixedBounds.pos.y - columnDisplayHeight) / itemHeight);

            if(selected == clickedItem){ // Make sure the same item was clicked twice
                if(OnSubmit) OnSubmit(items[selected], this);
            } else {
                selected = clickedItem;

                if(selected < 0) selected = 0;
                if(selected >= (int)items.size()) selected = items.size() - 1;
            }
        }
    }

    void ListView::OnMouseMove(vector2i_t mousePos){
        if(showScrollBar && sBar.pressed){
            sBar.OnMouseMoveRelative({0, mousePos.y - fixedBounds.pos.y});
        }
    }

    void ListView::OnMouseUp(__attribute__((unused)) vector2i_t mousePos){
        sBar.pressed = false;
    }

    void ListView::OnKeyPress(int key){
        assert(items.size() < INT32_MAX);

        switch(key){
            case KEY_ARROW_UP:
                selected--;
                break;
            case KEY_ARROW_DOWN:
                selected++;
                break;
            case KEY_ENTER:
                if(OnSubmit) OnSubmit(items[selected], this);
                return;
        }

        if(selected < 0) selected = 0;
        if(selected >= (int)items.size()) selected = items.size() - 1;
    }
    
    void ListView::ResetScrollBar(){
        assert(items.size() < INT32_MAX);

        if(((int)items.size() * itemHeight) > (fixedBounds.size.y - columnDisplayHeight)) showScrollBar = true;
        else showScrollBar = false;

        sBar.ResetScrollBar(fixedBounds.size.y - columnDisplayHeight, items.size() * itemHeight);
    }

    void ListView::UpdateFixedBounds(){
        Widget::UpdateFixedBounds();

        ResetScrollBar();
    }

    void GridView::ResetScrollBar(){
        if(!items.size()) return; // Lets not divide by zero
        
        assert(items.size() < INT32_MAX);

        if((static_cast<int>(items.size()) / itemsPerRow * itemSize.y) > fixedBounds.size.y) showScrollBar = true;
        else showScrollBar = false;

        if(showScrollBar)
            sBar.ResetScrollBar(fixedBounds.size.y, (items.size() / itemsPerRow) * itemSize.y);
    }

    void GridView::Paint(surface_t* surface){
        Graphics::DrawRect(fixedBounds, colours[Colour::ContentBackground], surface);

        int xPos = 0;
        int yPos = sBar.scrollPos ? -(sBar.scrollPos % itemSize.y) : 0;

        unsigned idx = 0;
        if(sBar.scrollPos){
            idx = sBar.scrollPos / itemSize.y * itemsPerRow;
        }

        for(; idx < items.size(); idx++){
            GridItem& item = items[idx];
            if(item.icon){
                vector2i_t pos = fixedBounds.pos + (vector2i_t){xPos + itemSize.x / 2 - 32, yPos + 2};
                rect_t srcRegion = {0, 0, 64, 64};

                if(pos.y < fixedBounds.pos.y){
                    srcRegion.height += (pos.y - fixedBounds.pos.y);
                    srcRegion.y -= (pos.y - fixedBounds.pos.y);
                    pos.y = fixedBounds.pos.y;
                }

                Graphics::surfacecpyTransparent(surface, item.icon, pos, srcRegion);
            }

            std::string str = item.name;
            int len = Graphics::GetTextLength(str.c_str());
            if(len > itemSize.x - 2) {
                int l = str.length() - 1;
                while(l){
                    str = str.substr(0, l);
                    len = Graphics::GetTextLength(str.c_str());

                    if(len < itemSize.x + 2) {
                        if(l > 2){
                            str.erase(str.end() - 1); // Omit last character
                            str.append("..."); // We have a variable width font should we should only have to omit 1 character
                        }
                        break;
                    }
                    
                    l = str.length() - 1;
                }
            }

            if(static_cast<int>(idx) == selected){
                Graphics::DrawRect(fixedBounds.x + xPos + (itemSize.x / 2) - (len / 2) - 4, fixedBounds.y + yPos + itemSize.y - Graphics::DefaultFont()->height - 4, len + 7, Graphics::DefaultFont()->height + 7, colours[Colour::Foreground], surface); // Highlight the label if selected
            }

            Graphics::DrawString(str.c_str(), fixedBounds.x + xPos + (itemSize.x / 2) - (len / 2), fixedBounds.y + yPos + itemSize.y - Graphics::DefaultFont()->height - 4, colours[Colour::Text], surface, fixedBounds);
            
            xPos += itemSize.x;

            if(xPos + itemSize.x >= fixedBounds.width){
                xPos = 0;
                yPos += itemSize.y;
            }

            if(yPos >= fixedBounds.height){
                break;
            }
        }

        if(showScrollBar){
            sBar.Paint(surface, {fixedBounds.x + fixedBounds.width - 16, fixedBounds.y});
        }
    }

    void GridView::OnMouseDown(vector2i_t mousePos){
        vector2i_t sBarPos = {fixedBounds.x + fixedBounds.width - 16, fixedBounds.y};
        if(showScrollBar && Graphics::PointInRect((rect_t){sBarPos, {16, fixedBounds.height}}, mousePos)){
            sBar.OnMouseDownRelative(mousePos - sBarPos);
        } else {
            selected = PosToItem(mousePos - fixedBounds.pos + (vector2i_t){sBar.scrollPos, 0}); // Position relative to position of GridView, but absolute from scroll position
        }
    }

    void GridView::OnMouseUp(vector2i_t mousePos){
        (void)mousePos;

        sBar.pressed = false;
    }

    void GridView::OnMouseMove(vector2i_t mousePos){
        vector2i_t sBarPos = {fixedBounds.x + fixedBounds.width - 16, fixedBounds.y};
        sBar.OnMouseMoveRelative(mousePos - sBarPos);
    }

    void GridView::OnDoubleClick(vector2i_t mousePos){
        selected = PosToItem(mousePos - fixedBounds.pos + (vector2i_t){sBar.scrollPos, 0}); // Position relative to position of GridView, but absolute from scroll position

        if(selected >= 0 && static_cast<unsigned>(selected) < items.size()){
            if(OnSelect) OnSubmit(items[selected], this);
        }
    }

    void GridView::OnKeyPress(int key){
        if(key == '\n'){
        if(selected >= 0 && static_cast<unsigned>(selected) < items.size()){
                if(OnSubmit) OnSubmit(items[selected], this);
            }
        } else if(key == KEY_ARROW_UP){
            if(selected / itemsPerRow > 0){
                selected -= itemsPerRow;

                if(selected >= 0 && static_cast<unsigned>(selected) < items.size()){
                    if(OnSelect) OnSelect(items[selected], this);
                }
            }
        } else if(key == KEY_ARROW_LEFT){
            if(selected > 0){
                selected--;

                if(selected >= 0 && static_cast<unsigned>(selected) < items.size()){
                    if(OnSelect) OnSelect(items[selected], this);
                }
            }
        } else if(key == KEY_ARROW_DOWN){
            if(selected / itemsPerRow < static_cast<int>(items.size()) / itemsPerRow){
                selected += itemsPerRow;

                if(selected >= 0 && static_cast<unsigned>(selected) < items.size()){
                    if(OnSelect) OnSelect(items[selected], this);
                }
            }
        } else if(key == KEY_ARROW_RIGHT){
            if(selected < static_cast<int>(items.size()) - 1){
                selected++;

                if(selected >= 0 && static_cast<unsigned>(selected) < items.size()){
                    if(OnSelect) OnSelect(items[selected], this);
                }
            }
        }

        if(selected >= 0 && static_cast<unsigned>(selected) < items.size()){
            if(selected >= 0 && ((selected / itemsPerRow + 1) * itemSize.y) > sBar.scrollPos + fixedBounds.height){ // Check if bottom of item is in view
                sBar.scrollPos = ((selected / itemsPerRow + 1) * itemSize.y) - fixedBounds.height; // Scroll down to selected item
            }

            if(selected >= 0 && (selected / itemsPerRow * itemSize.y) < sBar.scrollPos){
                sBar.scrollPos = (selected / itemsPerRow * itemSize.y); // Scroll up to selected item
            }
        }
    }
    
    int GridView::AddItem(GridItem& item){
        items.push_back(item);

        ResetScrollBar();
        
        return items.size() - 1;
    }

    void GridView::UpdateFixedBounds(){
        Widget::UpdateFixedBounds();

        itemsPerRow = fixedBounds.width / itemSize.x;

        ResetScrollBar();
    }

    void ScrollView::Paint(surface_t* surface){
        for(Widget* w : children){
            w->Paint(surface);
        }

        sBarVertical.Paint(surface, fixedBounds.pos + (vector2i_t){fixedBounds.size.x - 16, 0});
        sBarHorizontal.Paint(surface, fixedBounds.pos + (vector2i_t){0, fixedBounds.size.y - 16});
    }

    void ScrollView::AddWidget(Widget* w){
        children.push_back(w);

        w->SetParent(this);
        w->window = window;

        UpdateFixedBounds();
    }

    void ScrollView::OnMouseDown(vector2i_t mousePos){
        if(mousePos.x >= fixedBounds.width - 16){
            sBarVertical.OnMouseDownRelative(mousePos - (vector2i_t){fixedBounds.width - 16, 0});
        } else if(mousePos.y >= fixedBounds.height - 16){
            sBarHorizontal.OnMouseDownRelative(mousePos - (vector2i_t){0, fixedBounds.height - 16});
        } else for(Widget* w : children){
            if(Graphics::PointInRect(w->GetFixedBounds(), mousePos)){
                active = w;
                w->OnMouseDown(mousePos);
                break;
            }
        }
    }

    void ScrollView::OnMouseUp(vector2i_t mousePos){
        sBarHorizontal.pressed = sBarVertical.pressed = false;

        if(active){
            active->OnMouseUp(mousePos);
        }
    }

    void ScrollView::OnMouseMove(vector2i_t mousePos){
        if(sBarVertical.pressed){
            sBarVertical.OnMouseMoveRelative(mousePos - (vector2i_t){fixedBounds.width - 16, 0});
            UpdateFixedBounds();
        } else if(sBarHorizontal.pressed){
            sBarHorizontal.OnMouseMoveRelative(mousePos - (vector2i_t){0, fixedBounds.height - 16});
            UpdateFixedBounds();
        } else if(active){
            active->OnMouseMove(mousePos);
        }
    }

    void ScrollView::UpdateFixedBounds(){
        Widget::UpdateFixedBounds();

        rect_t realFixedBounds = fixedBounds;
        fixedBounds.pos += {-sBarHorizontal.scrollPos, -sBarVertical.scrollPos};
        fixedBounds.size += {sBarHorizontal.scrollPos, sBarVertical.scrollPos};

        for(Widget* w : children){
            w->UpdateFixedBounds();

            if(w->GetFixedBounds().width + w->GetFixedBounds().x > scrollBounds.width){
                scrollBounds.width = w->GetFixedBounds().width + w->GetFixedBounds().x;
            }

            if(w->GetFixedBounds().height + w->GetFixedBounds().y > scrollBounds.height){
                scrollBounds.height = w->GetFixedBounds().height + w->GetFixedBounds().y;
            }
        }

        fixedBounds = realFixedBounds;

        sBarVertical.ResetScrollBar(fixedBounds.height - 16, scrollBounds.height);
        sBarHorizontal.ResetScrollBar(fixedBounds.width - 16, scrollBounds.width);
    }
}