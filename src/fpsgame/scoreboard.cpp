// creation of scoreboard
#include "game.h"

namespace game
{
    VARP(scoreboard2d, 0, 1, 1);
    VARP(showservinfo, 0, 1, 1);
    VARP(showclientnum, 0, 0, 1);
    VARP(showpj, 0, 0, 1);
    VARP(showping, 0, 1, 1);
    VARP(showspectators, 0, 1, 1);
    VARP(highlightscore, 0, 1, 1);
    VARP(showconnecting, 0, 0, 1);

    static hashset<teaminfo> teaminfos;

    void clearteaminfo()
    {
        teaminfos.clear();
    }

    void setteaminfo(const char *team, int frags)
    {
        teaminfo *t = teaminfos.access(team);
        if(!t) { t = &teaminfos[team]; copystring(t->team, team, sizeof(t->team)); }
        t->frags = frags;
    }
            
    static inline bool playersort(const fpsent *a, const fpsent *b)
    {
        if(a->state==CS_SPECTATOR)
        {
            if(b->state==CS_SPECTATOR) return strcmp(a->name, b->name) < 0;
            else return false;
        }
        else if(b->state==CS_SPECTATOR) return true;
        if(m_ctf || m_collect)
        {
            if(a->flags > b->flags) return true;
            if(a->flags < b->flags) return false;
        }
        if(a->frags > b->frags) return true;
        if(a->frags < b->frags) return false;
        return strcmp(a->name, b->name) < 0;
    }

    void getbestplayers(vector<fpsent *> &best)
    {
        loopv(players)
        {
            fpsent *o = players[i];
            if(o->state!=CS_SPECTATOR) best.add(o);
        }
        best.sort(playersort);
        while(best.length() > 1 && best.last()->frags < best[0]->frags) best.drop();
    }

    void getbestteams(vector<const char *> &best)
    {
        if(cmode && cmode->hidefrags()) 
        {
            vector<teamscore> teamscores;
            cmode->getteamscores(teamscores);
            teamscores.sort(teamscore::compare);
            while(teamscores.length() > 1 && teamscores.last().score < teamscores[0].score) teamscores.drop();
            loopv(teamscores) best.add(teamscores[i].team);
        }
        else 
        {
            int bestfrags = INT_MIN;
            enumerates(teaminfos, teaminfo, t, bestfrags = max(bestfrags, t.frags));
            if(bestfrags <= 0) loopv(players)
            {
                fpsent *o = players[i];
                if(o->state!=CS_SPECTATOR && !teaminfos.access(o->team) && best.htfind(o->team) < 0) { bestfrags = 0; best.add(o->team); } 
            }
            enumerates(teaminfos, teaminfo, t, if(t.frags >= bestfrags) best.add(t.team));
        }
    }

    struct scoregroup : teamscore
    {
        vector<fpsent *> players;
    };
    static vector<scoregroup *> groups;
    static vector<fpsent *> spectators;

    static inline bool scoregroupcmp(const scoregroup *x, const scoregroup *y)
    {
        if(!x->team)
        {
            if(y->team) return false;
        }
        else if(!y->team) return true;
        if(x->score > y->score) return true;
        if(x->score < y->score) return false;
        if(x->players.length() > y->players.length()) return true;
        if(x->players.length() < y->players.length()) return false;
        return x->team && y->team && strcmp(x->team, y->team) < 0;
    }

    static int groupplayers()
    {
        int numgroups = 0;
        spectators.setsize(0);
        loopv(players)
        {
            fpsent *o = players[i];
            if(!showconnecting && !o->name[0]) continue;
            if(o->state==CS_SPECTATOR) { spectators.add(o); continue; }
            const char *team = m_teammode && o->team[0] ? o->team : NULL;
            bool found = false;
            loopj(numgroups)
            {
                scoregroup &g = *groups[j];
                if(team!=g.team && (!team || !g.team || strcmp(team, g.team))) continue;
                g.players.add(o);
                found = true;
            }
            if(found) continue;
            if(numgroups>=groups.length()) groups.add(new scoregroup);
            scoregroup &g = *groups[numgroups++];
            g.team = team;
            if(!team) g.score = 0;
            else if(cmode && cmode->hidefrags()) g.score = cmode->getteamscore(o->team);
            else { teaminfo *ti = teaminfos.access(team); g.score = ti ? ti->frags : 0; }
            g.players.setsize(0);
            g.players.add(o);
        }
        loopi(numgroups) groups[i]->players.sort(playersort);
        spectators.sort(playersort);
        groups.sort(scoregroupcmp, 0, numgroups);
        return numgroups;
    }

	VARP(scoreboardextinfo, 0, 1, 1);
	VARP(scoreboardfrags, 0, 1, 1);
	VARP(scoreboardflag, 0, 1, 1);
	VARP(scoreboardflags, 0, 1, 1);
	VARP(scoreboarddeaths, 0, 1, 1);
	VARP(scoreboardaccuracy, 0, 1, 1);
	VARP(scoreboardkpd, 0, 0, 1);
	VARP(scoreboardteamkills, 0, 0, 1);
	VARP(scoreboardcountry, 0, 3, 5);
	VARP(scoreboardspecping, 0, 1, 1);

    void renderscoreboard(g3d_gui &g, bool firstpass)
    {
		bool showextinfo = (lastmillis-lastinforesp < 20000) && scoreboardextinfo;
        const ENetAddress *address = connectedpeer();
        if(showservinfo && address)
        {
            string hostname;
            if(enet_address_get_host_ip(address, hostname, sizeof(hostname)) >= 0)
            {
                if(servinfo[0]) g.titlef("%.25s", 0xFFFF80, NULL, servinfo);
                else g.titlef("%s:%d", 0xFFFF80, NULL, hostname, address->port);
            }
        }
     
        g.pushlist();
        g.spring();
        g.text(server::modename(gamemode), 0xFFFF80);
        g.separator();
        const char *mname = getclientmap();
        g.text(mname[0] ? mname : "[new map]", 0xFFFF80);
        extern int gamespeed;
        if(gamespeed != 100) { g.separator(); g.textf("%d.%02dx", 0xFFFF80, NULL, gamespeed/100, gamespeed%100); }
        if(m_timed && mname[0] && (maplimit >= 0 || intermission))
        {
            g.separator();
            if(intermission) g.text("intermission", 0xFFFF80);
            else 
            {
                int secs = max(maplimit-lastmillis, 0)/1000, mins = secs/60;
                secs %= 60;
                g.pushlist();
                g.strut(mins >= 10 ? 4.5f : 3.5f);
                g.textf("%d:%02d", 0xFFFF80, NULL, mins, secs);
                g.poplist();
            }
        }
        if(ispaused()) { g.separator(); g.text("paused", 0xFFFF80); }
        g.spring();
        g.poplist();

        g.separator();
 
        int numgroups = groupplayers();
        loopk(numgroups)
        {
            if((k%2)==0) g.pushlist(); // horizontal
            
            scoregroup &sg = *groups[k];
            int bgcolor = sg.team && m_teammode ? (isteam(player1->team, sg.team) ? 0x3030C0 : 0xC03030) : 0,
                fgcolor = 0xFFFF80;

            g.pushlist(); // vertical
            g.pushlist(); // horizontal

            #define loopscoregroup(o, b) \
                loopv(sg.players) \
                { \
                    fpsent *o = sg.players[i]; \
                    b; \
                }    

            g.pushlist();
            if(sg.team && m_teammode)
            {
                g.pushlist();
                g.background(bgcolor, numgroups>1 ? 3 : 5);
                g.strut(1);
                g.poplist();
            }
            g.text("", 0, " ");
            loopscoregroup(o,
            {
                if(o==player1 && highlightscore && (multiplayer(false) || demoplayback || players.length() > 1))
                {
                    g.pushlist();
                    g.background(0x808080, numgroups>1 ? 3 : 5);
                }
                const playermodelinfo &mdl = getplayermodelinfo(o);
                const char *icon = sg.team && m_teammode ? (isteam(player1->team, sg.team) ? mdl.blueicon : mdl.redicon) : mdl.ffaicon;
				if (scoreboardflag && (m_ctf||m_protect) && (cmode->getflagowner(0) == o || cmode->getflagowner(1) == o)) icon = (isteam(o->team, player1->team)^m_protect? "blip_red_flag.png": "blip_blue_flag.png");
				g.text("", 0, icon);
                if(o==player1 && highlightscore && (multiplayer(false) || demoplayback || players.length() > 1)) g.poplist();
            });
            g.poplist();

            if(sg.team && m_teammode)
            {
                g.pushlist(); // vertical

                if(sg.score>=10000) g.textf("%s: WIN", fgcolor, NULL, sg.team);
                else g.textf("%s: %d", fgcolor, NULL, sg.team, sg.score);

                g.pushlist(); // horizontal
            }

            if(scoreboardfrags || !cmode || !cmode->hidefrags())
            {
				g.pushlist();
				g.strut(6);
				g.text("frags", fgcolor);
				loopscoregroup(o,
				{
					const char *ficon = NULL;
					if (scoreboardflags && o->flags) g.textf("%d-%d", 0xFFFFDD, NULL, o->frags, o->flags);
					else g.textf("%d", 0xFFFFDD, NULL, o->frags);
				});
				g.poplist();
            }

			if (showextinfo)
			{
				if(scoreboarddeaths)
				{
					g.pushlist();
					g.strut(6);
					g.text("deaths", fgcolor);
					loopscoregroup(o, g.textf("%d", 0xFFFFDD, NULL, o->deaths));
					g.poplist();
				}

				if(scoreboardaccuracy)
				{
					g.pushlist();
					g.strut(6);
					g.text("acc", fgcolor);
					loopscoregroup(o, g.textf("%d%%", 0xFFFFDD, NULL, o->ext_accuracy));
					g.poplist();
				}

				if(scoreboardkpd)
				{
					g.pushlist();
					g.strut(6);
					g.text("kpd", fgcolor);
					loopscoregroup(o, g.textf("%.2f", 0xFFFFDD, NULL, (float)o->frags/max(o->deaths, 1)));
					g.poplist();
				}

				if(scoreboardteamkills)
				{
					g.pushlist();
					g.strut(6);
					g.text("tks", fgcolor);
					loopscoregroup(o, g.textf("%d", 0xFFFFDD, NULL, o->ext_teamkills));
					g.poplist();
				}
			}

            g.pushlist();
            g.text("name", fgcolor);
            g.strut(13);
            loopscoregroup(o, 
            {
                int status = o->state!=CS_DEAD ? 0xFFFFDD : 0x606060;
                if(o->privilege)
                {
                    status = (o->privilege>=PRIV_ADMIN) ? 0xFF8000 : (o->privilege==PRIV_AUTH)? 0x9040FF: 0x40FF80;
                    if(o->state==CS_DEAD) status = (status>>1)&0x7F7F7F;
                }
                g.textf("%s ", status, NULL, colorname(o));
            });
            g.poplist();

            if(multiplayer(false) || demoplayback)
            {
                if(showpj)
                {
                    g.pushlist();
                    g.strut(6);
                    g.text("pj", fgcolor);
                    loopscoregroup(o,
                    {
                        if(o->state==CS_LAGGED) g.text("LAG", 0xFFFFDD);
                        else g.textf("%d", 0xFFFFDD, NULL, o->plag);
                    });
                    g.poplist();
                }

                if(showping)
                {
                    g.pushlist();
                    g.text("ping", fgcolor);
                    g.strut(6);
                    loopscoregroup(o,
                    {
                        fpsent *p = o->ownernum >= 0 ? getclient(o->ownernum) : o;
                        if(!p) p = o;
                        if(!showpj && p->state==CS_LAGGED) g.text("LAG", 0xFFFFDD);
                        else g.textf("%d", 0xFFFFDD, NULL, p->ping);
                    });
                    g.poplist();
                }
            }

            if(showclientnum || player1->privilege>=PRIV_MASTER)
            {
                g.pushlist();
				g.strut(3);
                g.text("cn", fgcolor);
                loopscoregroup(o, g.textf("%d", 0xFFFFDD, NULL, o->clientnum));
                g.poplist();
            }

			if(showextinfo && scoreboardcountry)
            {
                g.space(1);
				g.pushlist();
				g.strut(6);
				g.text("country", fgcolor);
				loopscoregroup(o,
								{
									const char icon[MAXSTRLEN] = "";
									const char *countrycode = GeoIP_country_code_by_ipnum(geoip, endianswap(o->ip));
									const char *country = (scoreboardcountry&2)? countrycode:
														  (scoreboardcountry&4)? GeoIP_country_name_by_ipnum(geoip, endianswap(o->ip)): "";
									if (scoreboardcountry&1) formatstring(icon)("%s.png", countrycode);
									g.textf("%s", 0xFFFFDD, (scoreboardcountry&1)? icon: NULL, country);
								}
							  );
				g.poplist();
            }
            
            if(sg.team && m_teammode)
            {
                g.poplist(); // horizontal
                g.poplist(); // vertical
            }

            g.poplist(); // horizontal
            g.poplist(); // vertical

            if(k+1<numgroups && (k+1)%2) g.space(2);
            else g.poplist(); // horizontal
        }
        
        if(showspectators && spectators.length())
        {
            if(showclientnum || player1->privilege>=PRIV_MASTER)
            {
                g.pushlist();
                
                g.pushlist();
                g.text("spectator", 0xFFFF80, " ");
                loopv(spectators) 
                {
                    fpsent *o = spectators[i];
                    int status = 0xFFFFDD;
                    if(o->privilege) status = o->privilege>=PRIV_ADMIN ? 0xFF8000 : 0x40FF80;
                    if(o==player1 && highlightscore)
                    {
                        g.pushlist();
                        g.background(0x808080, 3);
                    }
                    g.text(colorname(o), status, "spectator");
                    if(o==player1 && highlightscore) g.poplist();
                }
                g.poplist();

				if (scoreboardspecping)
				{
					g.space(1);
					g.pushlist();
					g.strut(3);
					g.text("ping", 0xFFFF80);
					loopv(spectators) g.textf("%d", 0xFFFFDD, NULL, spectators[i]->ping);
					g.poplist();
				}

                g.space(1);
                g.pushlist();
                g.strut(3);
				g.text("cn", 0xFFFF80);
                loopv(spectators) g.textf("%d", 0xFFFFDD, NULL, spectators[i]->clientnum);
                g.poplist();

				if(showextinfo && scoreboardcountry)
				{
					g.space(1);
					g.pushlist();
					g.text("country", 0xFFFF80);
					loopv(spectators)
									{
										const char icon[MAXSTRLEN] = "";
										const char *countrycode = GeoIP_country_code_by_ipnum(geoip, endianswap(spectators[i]->ip));
										const char *country = (scoreboardcountry&2)? countrycode:
															  (scoreboardcountry&4)? GeoIP_country_name_by_ipnum(geoip, endianswap(spectators[i]->ip)): "";
										if (scoreboardcountry&1) formatstring(icon)("%s.png", countrycode);
										g.textf("%s", 0xFFFFDD, (scoreboardcountry&1)? icon: NULL, country);
									}
					g.poplist();
				}

                g.poplist();
            }
            else
            {
                g.textf("%d spectator%s", 0xFFFF80, " ", spectators.length(), spectators.length()!=1 ? "s" : "");
                loopv(spectators)
                {
                    if((i%3)==0) 
                    {
                        g.pushlist();
                        g.text("", 0xFFFFDD, "spectator");
                    }
                    fpsent *o = spectators[i];
                    int status = 0xFFFFDD;
                    if(o->privilege) status = o->privilege>=PRIV_ADMIN ? 0xFF8000 : 0x40FF80;
                    if(o==player1 && highlightscore)
                    {
                        g.pushlist();
                        g.background(0x808080);
                    }
                    g.text(colorname(o), status);
                    if(o==player1 && highlightscore) g.poplist();
                    if(i+1<spectators.length() && (i+1)%3) g.space(1);
                    else g.poplist();
                }
            }
        }
    }

    struct scoreboardgui : g3d_callback
    {
        bool showing;
        vec menupos;
        int menustart;

        scoreboardgui() : showing(false) {}

        void show(bool on)
        {
            if(!showing && on)
            {
                menupos = menuinfrontofplayer();
                menustart = starttime();
            }
            showing = on;
        }

        void gui(g3d_gui &g, bool firstpass)
        {
            g.start(menustart, 0.03f, NULL, false);
            renderscoreboard(g, firstpass);
            g.end();
        }

        void render()
        {
            if(showing) g3d_addgui(this, menupos, (scoreboard2d ? GUI_FORCE_2D : GUI_2D | GUI_FOLLOW) | GUI_BOTTOM);
        }

    } scoreboard;

    void g3d_gamemenus()
    {
        scoreboard.render();
    }

    VARFN(scoreboard, showscoreboard, 0, 0, 1, scoreboard.show(showscoreboard!=0));

    void showscores(bool on)
    {
        showscoreboard = on ? 1 : 0;
        scoreboard.show(on);
    }
    ICOMMAND(showscores, "D", (int *down), showscores(*down!=0));

	VARP(whoisenabled, 0, 1, 1);
	struct whoisent { uint ip; vector<char *> names; };
	vector<whoisent> wies;

	void splitlist(const char *s, vector<char *> &elems)
	{
		const char *start = s, *end;
		while(end = strchr(start, ' '))
		{
			elems.add(newstring(start, end-start));
			start = end+1;
		}
		if (start[0]) elems.add(newstring(start, s+strlen(s)-start));
	}

	ICOMMAND(whoisentry, "ss", (const char *ip, const char *names),
	{
		if (!names) return;
		whoisent wie;
		wie.ip = endianswap(GeoIP_addr_to_num(ip));
		wie.ip %= (256*256*256);
		splitlist(names, wie.names);
		wies.add(wie);
	});

	vector<char *> *playernames(int cn)
	{
		fpsent *cl = getclient(cn);
		if (!cl) return NULL;
		int nip = cl->ip%(256*256*256);
		for (int i = 0; i < wies.length(); i++)
		{
			if (wies[i].ip == nip)
			{
				return &wies[i].names;
			}
		}
	}

	void addwhoisentry(uint ip, const char *name)
	{
		if (!scoreboardextinfo || !whoisenabled) return;
		int nip = ip%(256*256*256);
		bool found = false;
		for (int i = 0; i < wies.length(); i++) if (wies[i].ip == nip)
		{
			for (int j = 0; j < wies[i].names.length(); j++)
			{
				if (!strcmp(wies[i].names[j], name))
				{
					wies[i].names.remove(j);
					break;
				}
			}
			wies[i].names.insert(0, newstring(name));
			while (wies[i].names.length() > 15) delete[] wies[i].names.pop();
			found = true;
		}
		if (!found)
		{
			whoisent &wie = wies.add();
			wie.ip = nip;
			wie.names.add(newstring(name));
		}
	}

	void addplayerwhois(int cn)
	{
		fpsent *cl = getclient(cn);
		if (!cl || !cl->ip) return;
		addwhoisentry(cl->ip, cl->name);
	}

	VARP(globalwhois, 0, 1, 1);

	void whois(const int cn, bool msg)
	{
		if (!globalwhois && !msg) return;
		if (!scoreboardextinfo) { if (msg) conoutf("\f3error: whois requires extinfo to be enabled"); return; }
		if (!whoisenabled) { if (msg) conoutf("\f3error: whois is disabled"); return; }
		fpsent *cl = getclient(cn);
		if (!cl || !cl->ip) return;
		int nip = cl->ip%(256*256*256);
		for (int i = 0; i < wies.length(); i++)
		{
			if (wies[i].ip == nip)
			{
				defformatstring(line)("\f0possible names for \f1%s\f0 are: \f3", cl->name);
				loopvj(wies[i].names)
				{
					concatstring(line, wies[i].names[j]);
					concatstring(line, " ");
				}
				conoutf(line);
			}
		}
	}
	ICOMMAND(whois, "i", (const int *cn, bool msg), { whois(*cn, true); });

	//ICOMMAND(whoisip, "s", (const char *ip), 
	//{
	//	int nip = endianswap(GeoIP_addr_to_num(ip));
	//	for (int i = 0; i < wies.length(); i++)
	//	{
	//		if (wies[i].ip == nip)
	//		{
	//			defformatstring(line)("\f0possible names for IP \f1(%s)\f0 are: \f3", ip);
	//			loopvj(wies[i].names)
	//			{
	//				concatstring(line, wies[i].names[j]);
	//				concatstring(line, " ");
	//			}
	//			conoutf(line);
	//		}
	//	}
	//});

	void loadwhoisdb()
	{
		execfile("whois.cfg", false);
	}

	void saveclearwhoisdb()
	{
		stream *f = openutf8file(path("whois.cfg", true), "w");
		loopv(wies)
		{
			f->printf("whoisentry %s ", GeoIP_num_to_addr(endianswap(wies[i].ip)));
			char text[MAXTRANS];
			text[0] = '\0';
			for (int j = 0; j < wies[i].names.length(); j++)
			{
				concatstring(text, wies[i].names[j]);
				if (j != wies[i].names.length()-1) concatstring(text, " ");
			}
			f->putstring(escapestring(text));
			f->printf("\n");
			wies[i].names.deletearrays();
		}
		delete f;
		wies.setsize(0);
	}
}

