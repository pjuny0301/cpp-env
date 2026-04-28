import { createHashRouter } from "react-router";
import { Layout } from "./components/Layout";
import { Home } from "./components/Home";
import { DeckView } from "./components/DeckView";
import { DayIntro } from "./components/DayIntro";
import { QuizActive } from "./components/QuizActive";
import { QuizResults } from "./components/QuizResults";
import { AppProvider } from "./context";

// Create a wrapper to provide context to the routes
const ContextWrapper = ({ children }: { children: React.ReactNode }) => {
  return <AppProvider>{children}</AppProvider>;
};

export const router = createHashRouter([
  {
    path: "/",
    element: (
      <ContextWrapper>
        <Layout />
      </ContextWrapper>
    ),
    children: [
      { index: true, element: <Home /> },
      { path: "deck/:deckId", element: <DeckView /> },
      { path: "deck/:deckId/day/:dayId", element: <DayIntro /> },
      { path: "quiz/:deckId/:dayId", element: <QuizActive /> },
      { path: "results", element: <QuizResults /> },
      { path: "*", element: <div className="p-8 text-center text-slate-500">Not Found</div> }
    ],
  },
]);
